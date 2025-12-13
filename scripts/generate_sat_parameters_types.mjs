import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const ROOT = path.resolve(process.cwd());
const INPUT_PATH = path.join(ROOT, 'ortools/sat/sat_parameters.proto');
const OUTPUT_PATH = path.join(ROOT, 'javascript/lib/generated/sat_parameters.ts');

function stripComments(text) {
  // Keep line comments for JSDoc generation; only remove block comments.
  return text.replace(/\/\*[\s\S]*?\*\//g, '');
}

function tokenize(text) {
  const tokens = [];
  let i = 0;
  while (i < text.length) {
    const ch = text[i];
    if (ch === '/' && text[i + 1] === '/') {
      // Line comment.
      let j = i + 2;
      while (j < text.length && text[j] !== '\n') j++;
      const value = text.slice(i + 2, j).trim();
      tokens.push({ type: 'comment', value });
      i = j;
      continue;
    }
    if (/\s/.test(ch)) {
      i++;
      continue;
    }
    if (ch === '"' || ch === "'") {
      const quote = ch;
      let j = i + 1;
      let value = '';
      while (j < text.length) {
        const c = text[j];
        if (c === '\\') {
          value += c + (text[j + 1] ?? '');
          j += 2;
          continue;
        }
        if (c === quote) break;
        value += c;
        j++;
      }
      tokens.push({ type: 'string', value });
      i = j + 1;
      continue;
    }
    if (/[A-Za-z_]/.test(ch)) {
      let j = i + 1;
      while (j < text.length && /[A-Za-z0-9_\.]/.test(text[j])) j++;
      tokens.push({ type: 'ident', value: text.slice(i, j) });
      i = j;
      continue;
    }
    if (ch === '-' || /[0-9]/.test(ch)) {
      let j = i + 1;
      while (j < text.length && /[0-9]/.test(text[j])) j++;
      if (ch === '-' && j === i + 1) {
        tokens.push({ type: 'symbol', value: ch });
        i++;
        continue;
      }
      tokens.push({ type: 'number', value: text.slice(i, j) });
      i = j;
      continue;
    }
    tokens.push({ type: 'symbol', value: ch });
    i++;
  }
  return tokens;
}

function toLowerCamelCase(name) {
  // Protobufjs uses lowerCamelCase for field names.
  return name.replace(/_([a-zA-Z0-9])/g, (_, c) => String(c).toUpperCase());
}

function parse(tokens) {
  let pos = 0;

  const enums = [];
  const messages = [];

  const peek = () => tokens[pos];
  const next = () => tokens[pos++];
  const match = (value) => peek()?.value === value;

  const takeLeadingComments = () => {
    const comments = [];
    while (peek()?.type === 'comment') {
      const text = next().value.trim();
      if (text) comments.push(text);
    }
    return comments;
  };

  const skipNonSemantic = () => {
    while (peek()?.type === 'comment') pos++;
  };

  const expect = (value) => {
    skipNonSemantic();
    const tok = next();
    if (!tok || tok.value !== value) {
      throw new Error(`Expected "${value}" but got "${tok?.value ?? 'EOF'}"`);
    }
    return tok;
  };

  const expectIdent = () => {
    skipNonSemantic();
    const tok = next();
    if (!tok || tok.type !== 'ident') {
      throw new Error(`Expected identifier but got "${tok?.value ?? 'EOF'}"`);
    }
    return tok.value;
  };

  const skipUntil = (terminal) => {
    while (pos < tokens.length && !match(terminal)) pos++;
    if (match(terminal)) pos++;
  };

  const skipBrackets = (open, close) => {
    expect(open);
    let depth = 1;
    while (pos < tokens.length && depth > 0) {
      skipNonSemantic();
      const tok = next();
      if (!tok) break;
      if (tok.value === open) depth++;
      if (tok.value === close) depth--;
    }
  };

  const parseOptionsInBrackets = () => {
    const opts = {};
    expect('[');
    while (pos < tokens.length && !match(']')) {
      skipNonSemantic();
      if (peek()?.type !== 'ident') {
        pos++;
        continue;
      }
      const key = expectIdent();
      if (!match('=')) {
        pos++;
        continue;
      }
      expect('=');
      skipNonSemantic();
      let valueTok = next();
      if (!valueTok) break;
      // Support negative numbers: default = -1
      if (valueTok.value === '-' && peek()?.type === 'number') {
        valueTok = { type: 'number', value: `-${next().value}` };
      }
      if (key === 'default') {
        if (valueTok.type === 'string') {
          opts.default = JSON.stringify(valueTok.value);
        } else {
          opts.default = String(valueTok.value);
        }
      }
      // Consume trailing tokens until ',', ';' or ']'
      while (pos < tokens.length && !match(',') && !match(']')) {
        // Handle nested braces in options (rare); just skip.
        pos++;
      }
      if (match(',')) expect(',');
    }
    expect(']');
    return opts;
  };

  const parseEnum = (prefix, docsOverride = null) => {
    const docs = docsOverride ?? takeLeadingComments();
    expect('enum');
    const name = expectIdent();
    const fullName = prefix ? `${prefix}_${name}` : name;
    expect('{');
    const members = [];
    while (pos < tokens.length && !match('}')) {
      const memberDocs = takeLeadingComments();
      if (match('}')) break;
      if (match('option') || match('reserved')) {
        // Skip statements like: option allow_alias = true;
        // or reserved 2, 15 to 20;
        skipUntil(';');
        continue;
      }
      if (match(';')) {
        pos++;
        continue;
      }
      if (peek()?.type !== 'ident') {
        pos++;
        continue;
      }
      const memberName = expectIdent();
      if (!match('=')) {
        // Unexpected, skip line.
        skipUntil(';');
        continue;
      }
      expect('=');
      const numTok = next();
      if (!numTok || numTok.type !== 'number') {
        throw new Error(`Expected enum value number for ${fullName}.${memberName}`);
      }
      // Skip options.
      if (match('[')) parseOptionsInBrackets();
      if (match(';')) expect(';');
      members.push({ name: memberName, value: Number(numTok.value), docs: memberDocs });
    }
    expect('}');
    enums.push({ name: fullName, members, docs });
    return fullName;
  };

  const parseField = (containerName, forcedLabel = null, docsOverride = null) => {
    const docs = docsOverride ?? takeLeadingComments();
    if (match('}')) return null;
    let label = forcedLabel;
    if (!label && (match('optional') || match('repeated') || match('required'))) {
      label = next().value;
    }
    if (match('map')) {
      // map<key_type, value_type> field_name = 1;
      expect('map');
      expect('<');
      const keyType = expectIdent();
      expect(',');
      const valueType = expectIdent();
      expect('>');
      const fieldName = expectIdent();
      expect('=');
      // tag
      next();
      let options = null;
      if (match('[')) options = parseOptionsInBrackets();
      expect(';');
      return {
        containerName,
        label: label ?? 'optional',
        fieldName,
        kind: 'map',
        keyType,
        valueType,
        docs,
        options,
      };
    }

    // type field_name = 1 [..];
    if (peek()?.type !== 'ident') {
      // Not a field.
      skipUntil(';');
      return null;
    }
    const typeName = expectIdent();
    if (typeName === 'oneof') {
      // "oneof" handled elsewhere.
      return null;
    }
    if (peek()?.type !== 'ident') {
      skipUntil(';');
      return null;
    }
    const fieldName = expectIdent();
    if (!match('=')) {
      skipUntil(';');
      return null;
    }
    expect('=');
    // tag
    next();
    let options = null;
    if (match('[')) options = parseOptionsInBrackets();
    expect(';');
    return {
      containerName,
      label: label ?? 'optional',
      fieldName,
      kind: 'scalar',
      typeName,
      docs,
      options,
    };
  };

  const parseOneof = (containerName, fields) => {
    expect('oneof');
    // oneof name { ... }
    if (peek()?.type === 'ident') {
      next();
    }
    expect('{');
    while (pos < tokens.length && !match('}')) {
      if (match('option') || match('reserved')) {
        skipUntil(';');
        continue;
      }
      if (match(';')) {
        pos++;
        continue;
      }
      const field = parseField(containerName, /*forcedLabel*/ 'optional');
      if (field) fields.push(field);
    }
    expect('}');
  };

  const parseMessage = (prefix, docsOverride = null) => {
    const docs = docsOverride ?? takeLeadingComments();
    expect('message');
    const name = expectIdent();
    const fullName = prefix ? `${prefix}_${name}` : name;
    expect('{');
    const fields = [];
    while (pos < tokens.length && !match('}')) {
      const leadingDocs = takeLeadingComments();
      if (match('}')) break;
      if (match('message')) {
        parseMessage(fullName, leadingDocs);
        continue;
      }
      if (match('enum')) {
        parseEnum(fullName, leadingDocs);
        continue;
      }
      if (match('oneof')) {
        parseOneof(fullName, fields);
        continue;
      }
      if (match('extensions') || match('reserved') || match('option') || match('extend')) {
        skipUntil(';');
        continue;
      }
      if (match(';')) {
        pos++;
        continue;
      }
      const field = parseField(fullName, null, leadingDocs);
      if (field) fields.push(field);
    }
    expect('}');
    messages.push({ name: fullName, fields, docs });
    return fullName;
  };

  while (pos < tokens.length) {
    const leadingDocs = takeLeadingComments();
    if (match('message')) {
      parseMessage('', leadingDocs);
      continue;
    }
    if (match('enum')) {
      parseEnum('', leadingDocs);
      continue;
    }
    // Skip top-level statements like syntax/package/option/import.
    if (match('{')) {
      skipBrackets('{', '}');
      continue;
    }
    if (match(';')) {
      pos++;
      continue;
    }
    pos++;
  }

  return { enums, messages };
}

const PRIMITIVE_TS = new Map([
  ['double', 'number'],
  ['float', 'number'],
  ['int32', 'number'],
  ['int64', 'number'],
  ['uint32', 'number'],
  ['uint64', 'number'],
  ['sint32', 'number'],
  ['sint64', 'number'],
  ['fixed32', 'number'],
  ['fixed64', 'number'],
  ['sfixed32', 'number'],
  ['sfixed64', 'number'],
  ['bool', 'boolean'],
  ['string', 'string'],
  ['bytes', 'Uint8Array'],
]);

function normalizeTypeName(protoType) {
  const clean = protoType.startsWith('.') ? protoType.slice(1) : protoType;
  const parts = clean.split('.').filter(Boolean);
  if (parts.length === 0) return protoType;
  return parts.join('_');
}

function generateTs(ast) {
  const nameToEnum = new Map(ast.enums.map((e) => [e.name, e]));
  const nameToMessage = new Map(ast.messages.map((m) => [m.name, m]));

  const formatJSDoc = (docs, extraLines = []) => {
    const lines = [];
    const trimmed = (docs ?? []).map((d) => d.trim()).filter(Boolean);
    const extra = (extraLines ?? []).map((d) => d.trim()).filter(Boolean);
    if (trimmed.length === 0 && extra.length === 0) return null;
    lines.push('/**');
    for (const docLine of trimmed) {
      lines.push(` * ${docLine}`);
    }
    if (trimmed.length > 0 && extra.length > 0) {
      lines.push(' *');
    }
    for (const docLine of extra) {
      lines.push(` * ${docLine}`);
    }
    lines.push(' */');
    return lines.join('\n');
  };

  const lines = [];
  lines.push('/* eslint-disable @typescript-eslint/naming-convention */');
  lines.push('/*');
  lines.push(' * AUTO-GENERATED FILE.');
  lines.push(' *');
  lines.push(' * Source: ortools/sat/sat_parameters.proto');
  lines.push(' * Generator: scripts/generate_sat_parameters_types.mjs');
  lines.push(' */');
  lines.push('');

  const enumNamesInOrder = ast.enums.map((e) => e.name);
  for (const enumName of enumNamesInOrder) {
    const en = nameToEnum.get(enumName);
    if (!en) continue;
    const enumDoc = formatJSDoc(en.docs);
    if (enumDoc) lines.push(enumDoc);
    lines.push(`export enum ${enumName} {`);
    for (const member of en.members) {
      const memberDoc = formatJSDoc(member.docs);
      if (memberDoc) {
        for (const line of memberDoc.split('\n')) {
          lines.push(`  ${line}`);
        }
      }
      lines.push(`  ${member.name} = ${member.value},`);
    }
    lines.push('}');
    const unionName = `${enumName}_Name`;
    if (en.members.length > 0) {
      lines.push(`export type ${unionName} = ${en.members.map((m) => `'${m.name}'`).join(' | ')};`);
    } else {
      lines.push(`export type ${unionName} = never;`);
    }
    lines.push('');
  }

  const resolveTsType = (protoType, containerName = null) => {
    const primitive = PRIMITIVE_TS.get(protoType);
    if (primitive) return primitive;
    const normalized = normalizeTypeName(protoType);
    if (nameToEnum.has(normalized)) {
      return `${normalized} | ${normalized}_Name`;
    }
    if (nameToMessage.has(normalized)) {
      return normalized;
    }
    if (containerName) {
      const scoped = `${containerName}_${normalized}`;
      if (nameToEnum.has(scoped)) return `${scoped} | ${scoped}_Name`;
      if (nameToMessage.has(scoped)) return scoped;
    }
    // Unknown type from another .proto; keep it permissive but not "any".
    return 'unknown';
  };

  // Emit messages after enums so enum types can be referenced.
  const messageNamesInOrder = ast.messages.map((m) => m.name);
  for (const messageName of messageNamesInOrder) {
    const msg = nameToMessage.get(messageName);
    if (!msg) continue;
    const msgDoc = formatJSDoc(msg.docs);
    if (msgDoc) lines.push(msgDoc);
    lines.push(`export type ${messageName} = {`);
    for (const field of msg.fields) {
      const propName = toLowerCamelCase(field.fieldName);
      let tsType;
      if (field.kind === 'map') {
        const valueTs = resolveTsType(field.valueType, field.containerName);
        tsType = `Record<string, ${valueTs}>`;
      } else {
        tsType = resolveTsType(field.typeName, field.containerName);
      }
      if (field.label === 'repeated') {
        tsType = `Array<${tsType}>`;
      }
      const defaultValue = field.options?.default ? `@default ${field.options.default}` : null;
      const fieldDoc = formatJSDoc(field.docs, defaultValue ? [defaultValue] : []);
      if (fieldDoc) {
        for (const line of fieldDoc.split('\n')) {
          lines.push(`  ${line}`);
        }
      }
      lines.push(`  ${propName}?: ${tsType};`);
    }
    lines.push('};');
    lines.push('');
  }

  return lines.join('\n');
}

function ensureDir(dirPath) {
  fs.mkdirSync(dirPath, { recursive: true });
}

const input = fs.readFileSync(INPUT_PATH, 'utf8');
const stripped = stripComments(input);
const tokens = tokenize(stripped);
const ast = parse(tokens);

// Ensure SatParameters exists.
if (!ast.messages.some((m) => m.name === 'SatParameters')) {
  throw new Error('Did not find message SatParameters in sat_parameters.proto');
}

const output = generateTs(ast);
ensureDir(path.dirname(OUTPUT_PATH));
fs.writeFileSync(OUTPUT_PATH, output);
console.log(`Generated ${path.relative(ROOT, OUTPUT_PATH)}`);
