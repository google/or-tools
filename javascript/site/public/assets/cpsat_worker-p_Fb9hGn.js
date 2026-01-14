async function Jt(I = {}) {
  var A;
  (function() {
    function e(d) {
      d = d.split("-")[0];
      for (var _ = d.split(".").slice(0, 3); _.length < 3; ) _.push("00");
      return _ = _.map((v, g, m) => v.padStart(2, "0")), _.join("");
    }
    var r = (d) => [d / 1e4 | 0, (d / 100 | 0) % 100, d % 100].join("."), t = 2147483647, n = typeof process < "u" && process.versions?.node ? e(process.versions.node) : t;
    if (n < t)
      throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    if (n < 2147483647)
      throw new Error(`This emscripten-generated code requires node v${r(2147483647)} (detected v${r(n)})`);
    var i = typeof navigator < "u" && navigator.userAgent;
    if (i) {
      var a = i.includes("Safari/") && i.match(/Version\/(\d+\.?\d*\.?\d*)/) ? e(i.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : t;
      if (a < 15e4)
        throw new Error(`This emscripten-generated code requires Safari v${r(15e4)} (detected v${a})`);
      var s = i.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(i.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : t;
      if (s < 114)
        throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${s})`);
      var l = i.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(i.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : t;
      if (l < 85)
        throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${l})`);
    }
  })();
  var c = I, U = !!globalThis.window, $ = !!globalThis.WorkerGlobalScope, z = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", fe = !U && !z && !$, y = $ && self.name?.startsWith("em-pthread");
  y && (u(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0);
  var br = "./this.program", Sr = (e, r) => {
    throw r;
  }, Qt = import.meta.url, Ke = "";
  function en(e) {
    return c.locateFile ? c.locateFile(e, Ke) : Ke + e;
  }
  var Xe, Ne;
  if (!fe) if (U || $) {
    try {
      Ke = new URL(".", Qt).href;
    } catch {
    }
    if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    $ && (Ne = (e) => {
      var r = new XMLHttpRequest();
      return r.open("GET", e, !1), r.responseType = "arraybuffer", r.send(null), new Uint8Array(r.response);
    }), Xe = async (e) => {
      u(!kr(e), "readAsync does not work with file:// URLs");
      var r = await fetch(e, { credentials: "same-origin" });
      if (r.ok)
        return r.arrayBuffer();
      throw new Error(r.status + " : " + r.url);
    };
  } else
    throw new Error("environment detection error");
  var te = console.log.bind(console), b = console.error.bind(console);
  u(U || $ || z, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), u(!z, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable."), u(!fe, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
  var Ae;
  globalThis.WebAssembly || b("no native wasm support detected");
  var ie, _e = !1, me;
  function u(e, r) {
    e || P("Assertion failed" + (r ? ": " + r : ""));
  }
  var kr = (e) => e.startsWith("file://");
  function Tr() {
    var e = gr();
    u((e & 3) == 0), e == 0 && (e += 4), (f(), h)[e >> 2] = 34821223, (f(), h)[e + 4 >> 2] = 2310721022, (f(), h)[0] = 1668509029;
  }
  function Pe() {
    if (!_e) {
      var e = gr();
      e == 0 && (e += 4);
      var r = (f(), h)[e >> 2], t = (f(), h)[e + 4 >> 2];
      (r != 34821223 || t != 2310721022) && P(`Stack overflow! Stack cookie has been overwritten at ${de(e)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${de(t)} ${de(r)}`), (f(), h)[0] != 1668509029 && P("Runtime error: The application has corrupted its heap memory area (address zero)!");
    }
  }
  function Ye(...e) {
    console.warn(...e);
  }
  (() => {
    var e = new Int16Array(1), r = new Int8Array(e.buffer);
    e[0] = 25459, (r[0] !== 115 || r[1] !== 99) && P("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
  })();
  function Oe(e) {
    Object.getOwnPropertyDescriptor(c, e) || Object.defineProperty(c, e, { configurable: !0, set() {
      P(`Attempt to set \`Module.${e}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
    } });
  }
  function F(e) {
    return () => u(!1, `call to '${e}' via reference taken before Wasm module initialization`);
  }
  function rn(e) {
    Object.getOwnPropertyDescriptor(c, e) && P(`\`Module.${e}\` was supplied but \`${e}\` not included in INCOMING_MODULE_JS_API`);
  }
  function tn(e) {
    return e === "FS_createPath" || e === "FS_createDataFile" || e === "FS_createPreloadedFile" || e === "FS_preloadFile" || e === "FS_unlink" || e === "addRunDependency" || e === "FS_createLazyFile" || e === "FS_createDevice" || e === "removeRunDependency";
  }
  function nn(e) {
    Fr(e);
  }
  function Fr(e) {
    y || Object.getOwnPropertyDescriptor(c, e) || Object.defineProperty(c, e, { configurable: !0, get() {
      var r = `'${e}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
      tn(e) && (r += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), P(r);
    } });
  }
  function on() {
    function e() {
      var t = 0;
      return ve && typeof Se < "u" && (t = Se()), `w:${Ar},t:${de(t)}:`;
    }
    var r = Ye;
    Ye = (...t) => r(e(), ...t);
  }
  on();
  function f() {
    q.buffer != x.buffer && We();
  }
  var Je, Ze, Ar = 0, Pr;
  if (y) {
    var Qe = !1;
    self.onunhandledrejection = (r) => {
      throw r.reason || r;
    };
    async function e(r) {
      try {
        var t = r.data, n = t.cmd;
        if (n === "load") {
          Ar = t.workerID;
          let i = [];
          self.onmessage = (a) => i.push(a), Pr = () => {
            postMessage({ cmd: "loaded" });
            for (let a of i)
              e(a);
            self.onmessage = e;
          };
          for (const a of t.handlers)
            (!c[a] || c[a].proxy) && (c[a] = (...s) => {
              postMessage({ cmd: "callHandler", handler: a, args: s });
            }, a == "print" && (te = c[a]), a == "printErr" && (b = c[a]));
          q = t.wasmMemory, We(), ie = t.wasmModule, Dr(), qe();
        } else if (n === "run") {
          u(t.pthread_ptr), hn(t.pthread_ptr), pr(t.pthread_ptr, 0, 0, 1, 0, 0), w.threadInitTLS(), fr(t.pthread_ptr), Qe || (Lt(), Qe = !0);
          try {
            await qr(t.start_routine, t.arg);
          } catch (i) {
            if (i != "unwind")
              throw i;
          }
        } else t.target === "setimmediate" || (n === "checkMailbox" ? Qe && Be() : n && (b(`worker: received unknown command ${n}`), b(t)));
      } catch (i) {
        throw b(`worker: onmessage() captured an uncaught exception: ${i}`), i?.stack && b(i.stack), Bt(), i;
      }
    }
    self.onmessage = e;
  }
  var x, j, ae, Ce, R, h, Cr, Ie, N, Ir, ve = !1;
  function We() {
    var e = q.buffer;
    x = new Int8Array(e), ae = new Int16Array(e), c.HEAPU8 = j = new Uint8Array(e), Ce = new Uint16Array(e), R = new Int32Array(e), h = new Uint32Array(e), Cr = new Float32Array(e), Ie = new Float64Array(e), N = new BigInt64Array(e), Ir = new BigUint64Array(e);
  }
  function an() {
    if (!y) {
      if (c.wasmMemory)
        q = c.wasmMemory;
      else {
        var e = c.INITIAL_MEMORY || 16777216;
        u(e >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + e + "! (STACK_SIZE=65536)"), q = new WebAssembly.Memory({ initial: e / 65536, maximum: 32768, shared: !0 });
      }
      We();
    }
  }
  u(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
  function sn() {
    if (u(!y), c.preRun)
      for (typeof c.preRun == "function" && (c.preRun = [c.preRun]); c.preRun.length; )
        Ur(c.preRun.shift());
    Oe("preRun"), xr(Lr);
  }
  function Mr() {
    if (u(!ve), ve = !0, y) return Pr();
    Pe(), !c.noFSInit && !o.initialized && o.init(), re.__wasm_call_ctors(), o.ignorePermissions = !1;
  }
  function ln() {
    if (Pe(), !y) {
      if (c.postRun)
        for (typeof c.postRun == "function" && (c.postRun = [c.postRun]); c.postRun.length; )
          vn(c.postRun.shift());
      Oe("postRun"), xr(Vr);
    }
  }
  function P(e) {
    c.onAbort?.(e), e = "Aborted(" + e + ")", b(e), _e = !0;
    var r = new WebAssembly.RuntimeError(e);
    throw Ze?.(r), r;
  }
  function O(e, r) {
    return (...t) => {
      u(ve, `native function \`${e}\` called before runtime initialization`);
      var n = re[e];
      return u(n, `exported native function \`${e}\` not found`), u(t.length <= r, `native function \`${e}\` called with ${t.length} args but expects ${r}`), n(...t);
    };
  }
  var er;
  function dn() {
    return c.locateFile ? en("cp_sat_runtime.wasm") : new URL("/assets/cp_sat_runtime-CVEr9_T1.wasm", import.meta.url).href;
  }
  function cn(e) {
    if (e == er && Ae)
      return new Uint8Array(Ae);
    if (Ne)
      return Ne(e);
    throw "both async and sync fetching of the wasm failed";
  }
  async function un(e) {
    if (!Ae)
      try {
        var r = await Xe(e);
        return new Uint8Array(r);
      } catch {
      }
    return cn(e);
  }
  async function fn(e, r) {
    try {
      var t = await un(e), n = await WebAssembly.instantiate(t, r);
      return n;
    } catch (i) {
      b(`failed to asynchronously prepare wasm: ${i}`), kr(e) && b(`warning: Loading from a file URI (${e}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), P(i);
    }
  }
  async function _n(e, r, t) {
    if (!e)
      try {
        var n = fetch(r, { credentials: "same-origin" }), i = await WebAssembly.instantiateStreaming(n, t);
        return i;
      } catch (a) {
        b(`wasm streaming compile failed: ${a}`), b("falling back to ArrayBuffer instantiation");
      }
    return fn(r, t);
  }
  function Rr() {
    ti(), ke.__instrumented || (ke.__instrumented = !0, X.instrumentWasmImports(ke));
    var e = { env: ke, wasi_snapshot_preview1: ke };
    return e;
  }
  async function Dr() {
    function e(l, d) {
      return re = l.exports, re = X.instrumentWasmExports(re), pn(re._emscripten_tls_init), ri(re), ie = d, re;
    }
    var r = c;
    function t(l) {
      return u(c === r, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), r = null, e(l.instance, l.module);
    }
    var n = Rr();
    if (c.instantiateWasm)
      return new Promise((l, d) => {
        try {
          c.instantiateWasm(n, (_, v) => {
            l(e(_, v));
          });
        } catch (_) {
          b(`Module.instantiateWasm callback failed with error: ${_}`), d(_);
        }
      });
    if (y) {
      u(ie, "wasmModule should have been received via postMessage");
      var i = new WebAssembly.Instance(ie, Rr());
      return e(i, ie);
    }
    er ??= dn();
    var a = await _n(Ae, er, n), s = t(a);
    return s;
  }
  class Nr {
    name = "ExitStatus";
    constructor(r) {
      this.message = `Program terminated with exit(${r})`, this.status = r;
    }
  }
  var Or = (e) => {
    e.terminate(), e.onmessage = (r) => {
      var t = r.data.cmd;
      b(`received "${t}" command from terminated worker: ${e.workerID}`);
    };
  }, Wr = (e) => {
    u(!y, "Internal Error! cleanupThread() can only ever be called from main application thread!"), u(e, "Internal Error! Null pthread_ptr in cleanupThread!");
    var r = w.pthreads[e];
    u(r), w.returnWorkerToPool(r);
  }, xr = (e) => {
    for (; e.length > 0; )
      e.shift()(c);
  }, Lr = [], Ur = (e) => Lr.push(e), se = 0, Me = null, he = {}, le = null, $r = (e) => {
    if (se--, c.monitorRunDependencies?.(se), u(e, "removeRunDependency requires an ID"), u(he[e]), delete he[e], se == 0 && (le !== null && (clearInterval(le), le = null), Me)) {
      var r = Me;
      Me = null, r();
    }
  }, Br = (e) => {
    se++, c.monitorRunDependencies?.(se), u(e, "addRunDependency requires an ID"), u(!he[e]), he[e] = 1, le === null && globalThis.setInterval && (le = setInterval(() => {
      if (_e) {
        clearInterval(le), le = null;
        return;
      }
      var r = !1;
      for (var t in he)
        r || (r = !0, b("still waiting on run dependencies:")), b(`dependency: ${t}`);
      r && b("(end of list)");
    }, 1e4));
  }, zr = (e) => {
    u(!y, "Internal Error! spawnThread() can only ever be called from main application thread!"), u(e.pthread_ptr, "Internal error, no pthread ptr!");
    var r = w.getNewWorker();
    if (!r)
      return 6;
    u(!r.pthread_ptr, "Internal error!"), w.runningWorkers.push(r), w.pthreads[e.pthread_ptr] = r, r.pthread_ptr = e.pthread_ptr;
    var t = { cmd: "run", start_routine: e.startRoutine, arg: e.arg, pthread_ptr: e.pthread_ptr };
    return r.postMessage(t, e.transferList), 0;
  }, pe = 0, xe = () => ar || pe > 0, Hr = () => Er(), rr = (e) => qt(e), tr = (e) => Kt(e), W = (e, r, t, ...n) => {
    for (var i = n.length * 2, a = Hr(), s = tr(i * 8), l = s >> 3, d = 0; d < n.length; d++) {
      var _ = n[d];
      typeof _ == "bigint" ? ((f(), N)[l + 2 * d] = 1n, (f(), N)[l + 2 * d + 1] = _) : ((f(), N)[l + 2 * d] = 0n, (f(), Ie)[l + 2 * d + 1] = _);
    }
    var v = zt(e, r, i, s, t);
    return rr(a), v;
  };
  function nr(e) {
    if (y) return W(0, 0, 1, e);
    me = e, xe() || (w.terminateAllThreads(), c.onExit?.(e), _e = !0), Sr(e, new Nr(e));
  }
  function jr(e) {
    if (y) return W(1, 0, 0, e);
    or(e);
  }
  var mn = (e, r) => {
    if (me = e, oi(), y)
      throw u(!r), jr(e), "unwind";
    if (xe() && !r) {
      var t = `program exited (with status: ${e}), but keepRuntimeAlive() is set (counter=${pe}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
      Ze?.(t), b(t);
    }
    nr(e);
  }, or = mn, de = (e) => (u(typeof e == "number", `ptrToString expects a number, got ${typeof e}`), e >>>= 0, "0x" + e.toString(16).padStart(8, "0")), w = { unusedWorkers: [], runningWorkers: [], tlsInitFunctions: [], pthreads: {}, nextWorkerID: 1, init() {
    y || w.initMainThread();
  }, initMainThread() {
    for (var e = Math.max(1, Math.min(8, typeof navigator < "u" && navigator.hardwareConcurrency || 8)); e--; )
      w.allocateUnusedWorker();
    Ur(async () => {
      var r = w.loadWasmModuleToAllWorkers();
      Br("loading-workers"), await r, $r("loading-workers");
    });
  }, terminateAllThreads: () => {
    u(!y, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
    for (var e of w.runningWorkers)
      Or(e);
    for (var e of w.unusedWorkers)
      Or(e);
    w.unusedWorkers = [], w.runningWorkers = [], w.pthreads = {};
  }, returnWorkerToPool: (e) => {
    var r = e.pthread_ptr;
    delete w.pthreads[r], w.unusedWorkers.push(e), w.runningWorkers.splice(w.runningWorkers.indexOf(e), 1), e.pthread_ptr = 0, Ht(r);
  }, threadInitTLS() {
    w.tlsInitFunctions.forEach((e) => e());
  }, loadWasmModuleToWorker: (e) => new Promise((r) => {
    e.onmessage = (a) => {
      var s = a.data, l = s.cmd;
      if (s.targetThread && s.targetThread != Se()) {
        var d = w.pthreads[s.targetThread];
        d ? d.postMessage(s, s.transferList) : b(`Internal error! Worker sent a message "${l}" to target pthread ${s.targetThread}, but that thread no longer exists!`);
        return;
      }
      l === "checkMailbox" ? Be() : l === "spawnThread" ? zr(s) : l === "cleanupThread" ? Et(() => Wr(s.thread)) : l === "loaded" ? (e.loaded = !0, r(e)) : s.target === "setimmediate" ? e.postMessage(s) : l === "callHandler" ? c[s.handler](...s.args) : l && b(`worker sent an unknown command ${l}`);
    }, e.onerror = (a) => {
      var s = "worker sent an error!";
      throw e.pthread_ptr && (s = `Pthread ${de(e.pthread_ptr)} sent an error!`), b(`${s} ${a.filename}:${a.lineno}: ${a.message}`), a;
    }, u(q instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), u(ie instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
    var t = [], n = ["onExit", "onAbort", "print", "printErr"];
    for (var i of n)
      c.propertyIsEnumerable(i) && t.push(i);
    e.postMessage({ cmd: "load", handlers: t, wasmMemory: q, wasmModule: ie, workerID: e.workerID });
  }), async loadWasmModuleToAllWorkers() {
    return y ? void 0 : Promise.all(w.unusedWorkers.map(w.loadWasmModuleToWorker));
  }, allocateUnusedWorker() {
    var e;
    if (c.mainScriptUrlOrBlob) {
      var r = c.mainScriptUrlOrBlob;
      typeof r != "string" && (r = URL.createObjectURL(r)), e = new Worker(r, { type: "module", name: "em-pthread-" + w.nextWorkerID });
    } else e = new Worker(new URL(
      /* @vite-ignore */
      "/assets/cp_sat_runtime-0rfDU2yN.js",
      import.meta.url
    ), { type: "module", name: "em-pthread-" + w.nextWorkerID });
    e.workerID = w.nextWorkerID++, w.unusedWorkers.push(e);
  }, getNewWorker() {
    return w.unusedWorkers.length == 0 && (b("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.\nIf you want to throw an explicit error instead of the risk of deadlocking in those cases, use setting `-sPTHREAD_POOL_SIZE_STRICT=2`."), w.allocateUnusedWorker(), w.loadWasmModuleToWorker(w.unusedWorkers[0])), w.unusedWorkers.pop();
  } }, Vr = [], vn = (e) => Vr.push(e);
  function hn(e) {
    var r = (f(), h)[e + 52 >> 2], t = (f(), h)[e + 56 >> 2], n = r - t;
    u(r != 0), u(n != 0), u(r > n, "stackHigh must be higher then stackLow"), Gt(r, n), rr(r), Tr();
  }
  var ir = [], Gr = (e) => {
    var r = ir[e];
    return r || (ir[e] = r = Xt.get(e), X.isAsyncExport(r) && (ir[e] = r = X.makeAsyncFunction(r))), r;
  }, qr = async (e, r) => {
    pe = 0, ar = 0;
    var t = WebAssembly.promising(Gr(e))(r);
    Pe();
    function n(i) {
      if (xe()) {
        me = i;
        return;
      }
      yr(i);
    }
    t = await t, n(t);
  };
  qr.isAsync = !0;
  var ar = !0, pn = (e) => w.tlsInitFunctions.push(e), ne = (e) => {
    ne.shown ||= {}, ne.shown[e] || (ne.shown[e] = 1, b(e));
  }, q, Kr = globalThis.TextDecoder && new TextDecoder(), Xr = (e, r, t, n) => {
    var i = r + t;
    if (n) return i;
    for (; e[r] && !(r >= i); ) ++r;
    return r;
  }, ge = (e, r = 0, t, n) => {
    var i = Xr(e, r, t, n);
    if (i - r > 16 && e.buffer && Kr)
      return Kr.decode(e.buffer instanceof ArrayBuffer ? e.subarray(r, i) : e.slice(r, i));
    for (var a = ""; r < i; ) {
      var s = e[r++];
      if (!(s & 128)) {
        a += String.fromCharCode(s);
        continue;
      }
      var l = e[r++] & 63;
      if ((s & 224) == 192) {
        a += String.fromCharCode((s & 31) << 6 | l);
        continue;
      }
      var d = e[r++] & 63;
      if ((s & 240) == 224 ? s = (s & 15) << 12 | l << 6 | d : ((s & 248) != 240 && ne("Invalid UTF-8 leading byte " + de(s) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), s = (s & 7) << 18 | l << 12 | d << 6 | e[r++] & 63), s < 65536)
        a += String.fromCharCode(s);
      else {
        var _ = s - 65536;
        a += String.fromCharCode(55296 | _ >> 10, 56320 | _ & 1023);
      }
    }
    return a;
  }, J = (e, r, t) => (u(typeof e == "number", `UTF8ToString expects a number (got ${typeof e})`), e ? ge((f(), j), e, r, t) : ""), gn = (e, r, t, n) => P(`Assertion failed: ${J(e)}, at: ` + [r ? J(r) : "unknown filename", t, n ? J(n) : "unknown function"]);
  class yn {
    constructor(r) {
      this.excPtr = r, this.ptr = r - 24;
    }
    set_type(r) {
      (f(), h)[this.ptr + 4 >> 2] = r;
    }
    get_type() {
      return (f(), h)[this.ptr + 4 >> 2];
    }
    set_destructor(r) {
      (f(), h)[this.ptr + 8 >> 2] = r;
    }
    get_destructor() {
      return (f(), h)[this.ptr + 8 >> 2];
    }
    set_caught(r) {
      r = r ? 1 : 0, (f(), x)[this.ptr + 12] = r;
    }
    get_caught() {
      return (f(), x)[this.ptr + 12] != 0;
    }
    set_rethrown(r) {
      r = r ? 1 : 0, (f(), x)[this.ptr + 13] = r;
    }
    get_rethrown() {
      return (f(), x)[this.ptr + 13] != 0;
    }
    init(r, t) {
      this.set_adjusted_ptr(0), this.set_type(r), this.set_destructor(t);
    }
    set_adjusted_ptr(r) {
      (f(), h)[this.ptr + 16 >> 2] = r;
    }
    get_adjusted_ptr() {
      return (f(), h)[this.ptr + 16 >> 2];
    }
  }
  var En = (e, r, t) => {
    var n = new yn(e);
    n.init(r, t), u(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
  };
  function Yr(e, r, t, n) {
    return y ? W(2, 0, 1, e, r, t, n) : Jr(e, r, t, n);
  }
  var wn = () => !!globalThis.SharedArrayBuffer, Jr = (e, r, t, n) => {
    if (!wn())
      return Ye("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
    var i = [], a = 0;
    if (y && (i.length === 0 || a))
      return Yr(e, r, t, n);
    var s = { startRoutine: t, pthread_ptr: e, arg: n, transferList: i };
    return y ? (s.cmd = "spawnThread", postMessage(s, i), 0) : zr(s);
  }, Le = () => {
    u(C.varargs != null);
    var e = (f(), R)[+C.varargs >> 2];
    return C.varargs += 4, e;
  }, ye = Le, T = { isAbs: (e) => e.charAt(0) === "/", splitPath: (e) => {
    var r = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
    return r.exec(e).slice(1);
  }, normalizeArray: (e, r) => {
    for (var t = 0, n = e.length - 1; n >= 0; n--) {
      var i = e[n];
      i === "." ? e.splice(n, 1) : i === ".." ? (e.splice(n, 1), t++) : t && (e.splice(n, 1), t--);
    }
    if (r)
      for (; t; t--)
        e.unshift("..");
    return e;
  }, normalize: (e) => {
    var r = T.isAbs(e), t = e.slice(-1) === "/";
    return e = T.normalizeArray(e.split("/").filter((n) => !!n), !r).join("/"), !e && !r && (e = "."), e && t && (e += "/"), (r ? "/" : "") + e;
  }, dirname: (e) => {
    var r = T.splitPath(e), t = r[0], n = r[1];
    return !t && !n ? "." : (n && (n = n.slice(0, -1)), t + n);
  }, basename: (e) => e && e.match(/([^\/]+|\/)\/*$/)[1], join: (...e) => T.normalize(e.join("/")), join2: (e, r) => T.normalize(e + "/" + r) }, bn = () => (e) => e.set(crypto.getRandomValues(new Uint8Array(e.byteLength))), sr = (e) => {
    (sr = bn())(e);
  }, Ee = { resolve: (...e) => {
    for (var r = "", t = !1, n = e.length - 1; n >= -1 && !t; n--) {
      var i = n >= 0 ? e[n] : o.cwd();
      if (typeof i != "string")
        throw new TypeError("Arguments to path.resolve must be strings");
      if (!i)
        return "";
      r = i + "/" + r, t = T.isAbs(i);
    }
    return r = T.normalizeArray(r.split("/").filter((a) => !!a), !t).join("/"), (t ? "/" : "") + r || ".";
  }, relative: (e, r) => {
    e = Ee.resolve(e).slice(1), r = Ee.resolve(r).slice(1);
    function t(_) {
      for (var v = 0; v < _.length && _[v] === ""; v++)
        ;
      for (var g = _.length - 1; g >= 0 && _[g] === ""; g--)
        ;
      return v > g ? [] : _.slice(v, g - v + 1);
    }
    for (var n = t(e.split("/")), i = t(r.split("/")), a = Math.min(n.length, i.length), s = a, l = 0; l < a; l++)
      if (n[l] !== i[l]) {
        s = l;
        break;
      }
    for (var d = [], l = s; l < n.length; l++)
      d.push("..");
    return d = d.concat(i.slice(s)), d.join("/");
  } }, lr = [], ce = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.charCodeAt(t);
      n <= 127 ? r++ : n <= 2047 ? r += 2 : n >= 55296 && n <= 57343 ? (r += 4, ++t) : r += 3;
    }
    return r;
  }, Zr = (e, r, t, n) => {
    if (u(typeof e == "string", `stringToUTF8Array expects a string (got ${typeof e})`), !(n > 0)) return 0;
    for (var i = t, a = t + n - 1, s = 0; s < e.length; ++s) {
      var l = e.codePointAt(s);
      if (l <= 127) {
        if (t >= a) break;
        r[t++] = l;
      } else if (l <= 2047) {
        if (t + 1 >= a) break;
        r[t++] = 192 | l >> 6, r[t++] = 128 | l & 63;
      } else if (l <= 65535) {
        if (t + 2 >= a) break;
        r[t++] = 224 | l >> 12, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63;
      } else {
        if (t + 3 >= a) break;
        l > 1114111 && ne("Invalid Unicode code point " + de(l) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), r[t++] = 240 | l >> 18, r[t++] = 128 | l >> 12 & 63, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63, s++;
      }
    }
    return r[t] = 0, t - i;
  }, dr = (e, r, t) => {
    var n = ce(e) + 1, i = new Array(n), a = Zr(e, i, 0, i.length);
    return i.length = a, i;
  }, Sn = () => {
    if (!lr.length) {
      var e = null;
      if (globalThis.window?.prompt && (e = window.prompt("Input: "), e !== null && (e += `
`)), !e)
        return null;
      lr = dr(e);
    }
    return lr.shift();
  }, oe = { ttys: [], init() {
  }, shutdown() {
  }, register(e, r) {
    oe.ttys[e] = { input: [], output: [], ops: r }, o.registerDevice(e, oe.stream_ops);
  }, stream_ops: { open(e) {
    var r = oe.ttys[e.node.rdev];
    if (!r)
      throw new o.ErrnoError(43);
    e.tty = r, e.seekable = !1;
  }, close(e) {
    e.tty.ops.fsync(e.tty);
  }, fsync(e) {
    e.tty.ops.fsync(e.tty);
  }, read(e, r, t, n, i) {
    if (!e.tty || !e.tty.ops.get_char)
      throw new o.ErrnoError(60);
    for (var a = 0, s = 0; s < n; s++) {
      var l;
      try {
        l = e.tty.ops.get_char(e.tty);
      } catch {
        throw new o.ErrnoError(29);
      }
      if (l === void 0 && a === 0)
        throw new o.ErrnoError(6);
      if (l == null) break;
      a++, r[t + s] = l;
    }
    return a && (e.node.atime = Date.now()), a;
  }, write(e, r, t, n, i) {
    if (!e.tty || !e.tty.ops.put_char)
      throw new o.ErrnoError(60);
    try {
      for (var a = 0; a < n; a++)
        e.tty.ops.put_char(e.tty, r[t + a]);
    } catch {
      throw new o.ErrnoError(29);
    }
    return n && (e.node.mtime = e.node.ctime = Date.now()), a;
  } }, default_tty_ops: { get_char(e) {
    return Sn();
  }, put_char(e, r) {
    r === null || r === 10 ? (te(ge(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (te(ge(e.output)), e.output = []);
  }, ioctl_tcgets(e) {
    return { c_iflag: 25856, c_oflag: 5, c_cflag: 191, c_lflag: 35387, c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  }, ioctl_tcsets(e, r, t) {
    return 0;
  }, ioctl_tiocgwinsz(e) {
    return [24, 80];
  } }, default_tty1_ops: { put_char(e, r) {
    r === null || r === 10 ? (b(ge(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (b(ge(e.output)), e.output = []);
  } } }, kn = (e, r) => (f(), j).fill(0, e, e + r), Qr = (e, r) => (u(r, "alignment argument is required"), Math.ceil(e / r) * r), et = (e) => {
    e = Qr(e, 65536);
    var r = $t(65536, e);
    return r && kn(r, e), r;
  }, E = { ops_table: null, mount(e) {
    return E.createNode(null, "/", 16895, 0);
  }, createNode(e, r, t, n) {
    if (o.isBlkdev(t) || o.isFIFO(t))
      throw new o.ErrnoError(63);
    E.ops_table ||= { dir: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr, lookup: E.node_ops.lookup, mknod: E.node_ops.mknod, rename: E.node_ops.rename, unlink: E.node_ops.unlink, rmdir: E.node_ops.rmdir, readdir: E.node_ops.readdir, symlink: E.node_ops.symlink }, stream: { llseek: E.stream_ops.llseek } }, file: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr }, stream: { llseek: E.stream_ops.llseek, read: E.stream_ops.read, write: E.stream_ops.write, mmap: E.stream_ops.mmap, msync: E.stream_ops.msync } }, link: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr, readlink: E.node_ops.readlink }, stream: {} }, chrdev: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr }, stream: o.chrdev_stream_ops } };
    var i = o.createNode(e, r, t, n);
    return o.isDir(i.mode) ? (i.node_ops = E.ops_table.dir.node, i.stream_ops = E.ops_table.dir.stream, i.contents = {}) : o.isFile(i.mode) ? (i.node_ops = E.ops_table.file.node, i.stream_ops = E.ops_table.file.stream, i.usedBytes = 0, i.contents = null) : o.isLink(i.mode) ? (i.node_ops = E.ops_table.link.node, i.stream_ops = E.ops_table.link.stream) : o.isChrdev(i.mode) && (i.node_ops = E.ops_table.chrdev.node, i.stream_ops = E.ops_table.chrdev.stream), i.atime = i.mtime = i.ctime = Date.now(), e && (e.contents[r] = i, e.atime = e.mtime = e.ctime = i.atime), i;
  }, getFileDataAsTypedArray(e) {
    return e.contents ? e.contents.subarray ? e.contents.subarray(0, e.usedBytes) : new Uint8Array(e.contents) : new Uint8Array(0);
  }, expandFileStorage(e, r) {
    var t = e.contents ? e.contents.length : 0;
    if (!(t >= r)) {
      var n = 1024 * 1024;
      r = Math.max(r, t * (t < n ? 2 : 1.125) >>> 0), t != 0 && (r = Math.max(r, 256));
      var i = e.contents;
      e.contents = new Uint8Array(r), e.usedBytes > 0 && e.contents.set(i.subarray(0, e.usedBytes), 0);
    }
  }, resizeFileStorage(e, r) {
    if (e.usedBytes != r)
      if (r == 0)
        e.contents = null, e.usedBytes = 0;
      else {
        var t = e.contents;
        e.contents = new Uint8Array(r), t && e.contents.set(t.subarray(0, Math.min(r, e.usedBytes))), e.usedBytes = r;
      }
  }, node_ops: { getattr(e) {
    var r = {};
    return r.dev = o.isChrdev(e.mode) ? e.id : 1, r.ino = e.id, r.mode = e.mode, r.nlink = 1, r.uid = 0, r.gid = 0, r.rdev = e.rdev, o.isDir(e.mode) ? r.size = 4096 : o.isFile(e.mode) ? r.size = e.usedBytes : o.isLink(e.mode) ? r.size = e.link.length : r.size = 0, r.atime = new Date(e.atime), r.mtime = new Date(e.mtime), r.ctime = new Date(e.ctime), r.blksize = 4096, r.blocks = Math.ceil(r.size / r.blksize), r;
  }, setattr(e, r) {
    for (const t of ["mode", "atime", "mtime", "ctime"])
      r[t] != null && (e[t] = r[t]);
    r.size !== void 0 && E.resizeFileStorage(e, r.size);
  }, lookup(e, r) {
    throw new o.ErrnoError(44);
  }, mknod(e, r, t, n) {
    return E.createNode(e, r, t, n);
  }, rename(e, r, t) {
    var n;
    try {
      n = o.lookupNode(r, t);
    } catch {
    }
    if (n) {
      if (o.isDir(e.mode))
        for (var i in n.contents)
          throw new o.ErrnoError(55);
      o.hashRemoveNode(n);
    }
    delete e.parent.contents[e.name], r.contents[t] = e, e.name = t, r.ctime = r.mtime = e.parent.ctime = e.parent.mtime = Date.now();
  }, unlink(e, r) {
    delete e.contents[r], e.ctime = e.mtime = Date.now();
  }, rmdir(e, r) {
    var t = o.lookupNode(e, r);
    for (var n in t.contents)
      throw new o.ErrnoError(55);
    delete e.contents[r], e.ctime = e.mtime = Date.now();
  }, readdir(e) {
    return [".", "..", ...Object.keys(e.contents)];
  }, symlink(e, r, t) {
    var n = E.createNode(e, r, 41471, 0);
    return n.link = t, n;
  }, readlink(e) {
    if (!o.isLink(e.mode))
      throw new o.ErrnoError(28);
    return e.link;
  } }, stream_ops: { read(e, r, t, n, i) {
    var a = e.node.contents;
    if (i >= e.node.usedBytes) return 0;
    var s = Math.min(e.node.usedBytes - i, n);
    if (u(s >= 0), s > 8 && a.subarray)
      r.set(a.subarray(i, i + s), t);
    else
      for (var l = 0; l < s; l++) r[t + l] = a[i + l];
    return s;
  }, write(e, r, t, n, i, a) {
    if (u(!(r instanceof ArrayBuffer)), r.buffer === (f(), x).buffer && (a = !1), !n) return 0;
    var s = e.node;
    if (s.mtime = s.ctime = Date.now(), r.subarray && (!s.contents || s.contents.subarray)) {
      if (a)
        return u(i === 0, "canOwn must imply no weird position inside the file"), s.contents = r.subarray(t, t + n), s.usedBytes = n, n;
      if (s.usedBytes === 0 && i === 0)
        return s.contents = r.slice(t, t + n), s.usedBytes = n, n;
      if (i + n <= s.usedBytes)
        return s.contents.set(r.subarray(t, t + n), i), n;
    }
    if (E.expandFileStorage(s, i + n), s.contents.subarray && r.subarray)
      s.contents.set(r.subarray(t, t + n), i);
    else
      for (var l = 0; l < n; l++)
        s.contents[i + l] = r[t + l];
    return s.usedBytes = Math.max(s.usedBytes, i + n), n;
  }, llseek(e, r, t) {
    var n = r;
    if (t === 1 ? n += e.position : t === 2 && o.isFile(e.node.mode) && (n += e.node.usedBytes), n < 0)
      throw new o.ErrnoError(28);
    return n;
  }, mmap(e, r, t, n, i) {
    if (!o.isFile(e.node.mode))
      throw new o.ErrnoError(43);
    var a, s, l = e.node.contents;
    if (!(i & 2) && l && l.buffer === (f(), x).buffer)
      s = !1, a = l.byteOffset;
    else {
      if (s = !0, a = et(r), !a)
        throw new o.ErrnoError(48);
      l && ((t > 0 || t + r < l.length) && (l.subarray ? l = l.subarray(t, t + r) : l = Array.prototype.slice.call(l, t, t + r)), (f(), x).set(l, a));
    }
    return { ptr: a, allocated: s };
  }, msync(e, r, t, n, i) {
    return E.stream_ops.write(e, r, 0, n, t, !1), 0;
  } } }, Tn = (e) => {
    var r = { r: 0, "r+": 2, w: 577, "w+": 578, a: 1089, "a+": 1090 }, t = r[e];
    if (typeof t > "u")
      throw new Error(`Unknown file open mode: ${e}`);
    return t;
  }, cr = (e, r) => {
    var t = 0;
    return e && (t |= 365), r && (t |= 146), t;
  }, Fn = (e) => J(Ut(e)), rt = { EPERM: 63, ENOENT: 44, ESRCH: 71, EINTR: 27, EIO: 29, ENXIO: 60, E2BIG: 1, ENOEXEC: 45, EBADF: 8, ECHILD: 12, EAGAIN: 6, EWOULDBLOCK: 6, ENOMEM: 48, EACCES: 2, EFAULT: 21, ENOTBLK: 105, EBUSY: 10, EEXIST: 20, EXDEV: 75, ENODEV: 43, ENOTDIR: 54, EISDIR: 31, EINVAL: 28, ENFILE: 41, EMFILE: 33, ENOTTY: 59, ETXTBSY: 74, EFBIG: 22, ENOSPC: 51, ESPIPE: 70, EROFS: 69, EMLINK: 34, EPIPE: 64, EDOM: 18, ERANGE: 68, ENOMSG: 49, EIDRM: 24, ECHRNG: 106, EL2NSYNC: 156, EL3HLT: 107, EL3RST: 108, ELNRNG: 109, EUNATCH: 110, ENOCSI: 111, EL2HLT: 112, EDEADLK: 16, ENOLCK: 46, EBADE: 113, EBADR: 114, EXFULL: 115, ENOANO: 104, EBADRQC: 103, EBADSLT: 102, EDEADLOCK: 16, EBFONT: 101, ENOSTR: 100, ENODATA: 116, ETIME: 117, ENOSR: 118, ENONET: 119, ENOPKG: 120, EREMOTE: 121, ENOLINK: 47, EADV: 122, ESRMNT: 123, ECOMM: 124, EPROTO: 65, EMULTIHOP: 36, EDOTDOT: 125, EBADMSG: 9, ENOTUNIQ: 126, EBADFD: 127, EREMCHG: 128, ELIBACC: 129, ELIBBAD: 130, ELIBSCN: 131, ELIBMAX: 132, ELIBEXEC: 133, ENOSYS: 52, ENOTEMPTY: 55, ENAMETOOLONG: 37, ELOOP: 32, EOPNOTSUPP: 138, EPFNOSUPPORT: 139, ECONNRESET: 15, ENOBUFS: 42, EAFNOSUPPORT: 5, EPROTOTYPE: 67, ENOTSOCK: 57, ENOPROTOOPT: 50, ESHUTDOWN: 140, ECONNREFUSED: 14, EADDRINUSE: 3, ECONNABORTED: 13, ENETUNREACH: 40, ENETDOWN: 38, ETIMEDOUT: 73, EHOSTDOWN: 142, EHOSTUNREACH: 23, EINPROGRESS: 26, EALREADY: 7, EDESTADDRREQ: 17, EMSGSIZE: 35, EPROTONOSUPPORT: 66, ESOCKTNOSUPPORT: 137, EADDRNOTAVAIL: 4, ENETRESET: 39, EISCONN: 30, ENOTCONN: 53, ETOOMANYREFS: 141, EUSERS: 136, EDQUOT: 19, ESTALE: 72, ENOTSUP: 138, ENOMEDIUM: 148, EILSEQ: 25, EOVERFLOW: 61, ECANCELED: 11, ENOTRECOVERABLE: 56, EOWNERDEAD: 62, ESTRPIPE: 135 }, An = async (e) => {
    var r = await Xe(e);
    return u(r, `Loading data file "${e}" failed (no arrayBuffer).`), new Uint8Array(r);
  }, Pn = (...e) => o.createDataFile(...e), Cn = (e) => {
    for (var r = e; ; ) {
      if (!he[e]) return e;
      e = r + Math.random();
    }
  }, tt = [], In = async (e, r) => {
    typeof Browser < "u" && Browser.init();
    for (var t of tt)
      if (t.canHandle(r))
        return u(t.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), t.handle(e, r);
    return e;
  }, nt = async (e, r, t, n, i, a, s, l) => {
    var d = r ? Ee.resolve(T.join2(e, r)) : e, _ = Cn(`cp ${d}`);
    Br(_);
    try {
      var v = t;
      typeof t == "string" && (v = await An(t)), v = await In(v, d), l?.(), a || Pn(e, r, v, n, i, s);
    } finally {
      $r(_);
    }
  }, Mn = (e, r, t, n, i, a, s, l, d, _) => {
    nt(e, r, t, n, i, l, d, _).then(a).catch(s);
  }, o = { root: null, mounts: [], devices: {}, streams: [], nextInode: 1, nameTable: null, currentPath: "/", initialized: !1, ignorePermissions: !0, filesystems: null, syncFSRequests: 0, readFiles: {}, ErrnoError: class extends Error {
    name = "ErrnoError";
    constructor(e) {
      super(ve ? Fn(e) : ""), this.errno = e;
      for (var r in rt)
        if (rt[r] === e) {
          this.code = r;
          break;
        }
    }
  }, FSStream: class {
    shared = {};
    get object() {
      return this.node;
    }
    set object(e) {
      this.node = e;
    }
    get isRead() {
      return (this.flags & 2097155) !== 1;
    }
    get isWrite() {
      return (this.flags & 2097155) !== 0;
    }
    get isAppend() {
      return this.flags & 1024;
    }
    get flags() {
      return this.shared.flags;
    }
    set flags(e) {
      this.shared.flags = e;
    }
    get position() {
      return this.shared.position;
    }
    set position(e) {
      this.shared.position = e;
    }
  }, FSNode: class {
    node_ops = {};
    stream_ops = {};
    readMode = 365;
    writeMode = 146;
    mounted = null;
    constructor(e, r, t, n) {
      e || (e = this), this.parent = e, this.mount = e.mount, this.id = o.nextInode++, this.name = r, this.mode = t, this.rdev = n, this.atime = this.mtime = this.ctime = Date.now();
    }
    get read() {
      return (this.mode & this.readMode) === this.readMode;
    }
    set read(e) {
      e ? this.mode |= this.readMode : this.mode &= ~this.readMode;
    }
    get write() {
      return (this.mode & this.writeMode) === this.writeMode;
    }
    set write(e) {
      e ? this.mode |= this.writeMode : this.mode &= ~this.writeMode;
    }
    get isFolder() {
      return o.isDir(this.mode);
    }
    get isDevice() {
      return o.isChrdev(this.mode);
    }
  }, lookupPath(e, r = {}) {
    if (!e)
      throw new o.ErrnoError(44);
    r.follow_mount ??= !0, T.isAbs(e) || (e = o.cwd() + "/" + e);
    e: for (var t = 0; t < 40; t++) {
      for (var n = e.split("/").filter((_) => !!_), i = o.root, a = "/", s = 0; s < n.length; s++) {
        var l = s === n.length - 1;
        if (l && r.parent)
          break;
        if (n[s] !== ".") {
          if (n[s] === "..") {
            if (a = T.dirname(a), o.isRoot(i)) {
              e = a + "/" + n.slice(s + 1).join("/"), t--;
              continue e;
            } else
              i = i.parent;
            continue;
          }
          a = T.join2(a, n[s]);
          try {
            i = o.lookupNode(i, n[s]);
          } catch (_) {
            if (_?.errno === 44 && l && r.noent_okay)
              return { path: a };
            throw _;
          }
          if (o.isMountpoint(i) && (!l || r.follow_mount) && (i = i.mounted.root), o.isLink(i.mode) && (!l || r.follow)) {
            if (!i.node_ops.readlink)
              throw new o.ErrnoError(52);
            var d = i.node_ops.readlink(i);
            T.isAbs(d) || (d = T.dirname(a) + "/" + d), e = d + "/" + n.slice(s + 1).join("/");
            continue e;
          }
        }
      }
      return { path: a, node: i };
    }
    throw new o.ErrnoError(32);
  }, getPath(e) {
    for (var r; ; ) {
      if (o.isRoot(e)) {
        var t = e.mount.mountpoint;
        return r ? t[t.length - 1] !== "/" ? `${t}/${r}` : t + r : t;
      }
      r = r ? `${e.name}/${r}` : e.name, e = e.parent;
    }
  }, hashName(e, r) {
    for (var t = 0, n = 0; n < r.length; n++)
      t = (t << 5) - t + r.charCodeAt(n) | 0;
    return (e + t >>> 0) % o.nameTable.length;
  }, hashAddNode(e) {
    var r = o.hashName(e.parent.id, e.name);
    e.name_next = o.nameTable[r], o.nameTable[r] = e;
  }, hashRemoveNode(e) {
    var r = o.hashName(e.parent.id, e.name);
    if (o.nameTable[r] === e)
      o.nameTable[r] = e.name_next;
    else
      for (var t = o.nameTable[r]; t; ) {
        if (t.name_next === e) {
          t.name_next = e.name_next;
          break;
        }
        t = t.name_next;
      }
  }, lookupNode(e, r) {
    var t = o.mayLookup(e);
    if (t)
      throw new o.ErrnoError(t);
    for (var n = o.hashName(e.id, r), i = o.nameTable[n]; i; i = i.name_next) {
      var a = i.name;
      if (i.parent.id === e.id && a === r)
        return i;
    }
    return o.lookup(e, r);
  }, createNode(e, r, t, n) {
    u(typeof e == "object");
    var i = new o.FSNode(e, r, t, n);
    return o.hashAddNode(i), i;
  }, destroyNode(e) {
    o.hashRemoveNode(e);
  }, isRoot(e) {
    return e === e.parent;
  }, isMountpoint(e) {
    return !!e.mounted;
  }, isFile(e) {
    return (e & 61440) === 32768;
  }, isDir(e) {
    return (e & 61440) === 16384;
  }, isLink(e) {
    return (e & 61440) === 40960;
  }, isChrdev(e) {
    return (e & 61440) === 8192;
  }, isBlkdev(e) {
    return (e & 61440) === 24576;
  }, isFIFO(e) {
    return (e & 61440) === 4096;
  }, isSocket(e) {
    return (e & 49152) === 49152;
  }, flagsToPermissionString(e) {
    var r = ["r", "w", "rw"][e & 3];
    return e & 512 && (r += "w"), r;
  }, nodePermissions(e, r) {
    return o.ignorePermissions ? 0 : r.includes("r") && !(e.mode & 292) || r.includes("w") && !(e.mode & 146) || r.includes("x") && !(e.mode & 73) ? 2 : 0;
  }, mayLookup(e) {
    if (!o.isDir(e.mode)) return 54;
    var r = o.nodePermissions(e, "x");
    return r || (e.node_ops.lookup ? 0 : 2);
  }, mayCreate(e, r) {
    if (!o.isDir(e.mode))
      return 54;
    try {
      var t = o.lookupNode(e, r);
      return 20;
    } catch {
    }
    return o.nodePermissions(e, "wx");
  }, mayDelete(e, r, t) {
    var n;
    try {
      n = o.lookupNode(e, r);
    } catch (a) {
      return a.errno;
    }
    var i = o.nodePermissions(e, "wx");
    if (i)
      return i;
    if (t) {
      if (!o.isDir(n.mode))
        return 54;
      if (o.isRoot(n) || o.getPath(n) === o.cwd())
        return 10;
    } else if (o.isDir(n.mode))
      return 31;
    return 0;
  }, mayOpen(e, r) {
    return e ? o.isLink(e.mode) ? 32 : o.isDir(e.mode) && (o.flagsToPermissionString(r) !== "r" || r & 576) ? 31 : o.nodePermissions(e, o.flagsToPermissionString(r)) : 44;
  }, checkOpExists(e, r) {
    if (!e)
      throw new o.ErrnoError(r);
    return e;
  }, MAX_OPEN_FDS: 4096, nextfd() {
    for (var e = 0; e <= o.MAX_OPEN_FDS; e++)
      if (!o.streams[e])
        return e;
    throw new o.ErrnoError(33);
  }, getStreamChecked(e) {
    var r = o.getStream(e);
    if (!r)
      throw new o.ErrnoError(8);
    return r;
  }, getStream: (e) => o.streams[e], createStream(e, r = -1) {
    return u(r >= -1), e = Object.assign(new o.FSStream(), e), r == -1 && (r = o.nextfd()), e.fd = r, o.streams[r] = e, e;
  }, closeStream(e) {
    o.streams[e] = null;
  }, dupStream(e, r = -1) {
    var t = o.createStream(e, r);
    return t.stream_ops?.dup?.(t), t;
  }, doSetAttr(e, r, t) {
    var n = e?.stream_ops.setattr, i = n ? e : r;
    n ??= r.node_ops.setattr, o.checkOpExists(n, 63), n(i, t);
  }, chrdev_stream_ops: { open(e) {
    var r = o.getDevice(e.node.rdev);
    e.stream_ops = r.stream_ops, e.stream_ops.open?.(e);
  }, llseek() {
    throw new o.ErrnoError(70);
  } }, major: (e) => e >> 8, minor: (e) => e & 255, makedev: (e, r) => e << 8 | r, registerDevice(e, r) {
    o.devices[e] = { stream_ops: r };
  }, getDevice: (e) => o.devices[e], getMounts(e) {
    for (var r = [], t = [e]; t.length; ) {
      var n = t.pop();
      r.push(n), t.push(...n.mounts);
    }
    return r;
  }, syncfs(e, r) {
    typeof e == "function" && (r = e, e = !1), o.syncFSRequests++, o.syncFSRequests > 1 && b(`warning: ${o.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
    var t = o.getMounts(o.root.mount), n = 0;
    function i(l) {
      return u(o.syncFSRequests > 0), o.syncFSRequests--, r(l);
    }
    function a(l) {
      if (l)
        return a.errored ? void 0 : (a.errored = !0, i(l));
      ++n >= t.length && i(null);
    }
    for (var s of t)
      s.type.syncfs ? s.type.syncfs(s, e, a) : a(null);
  }, mount(e, r, t) {
    if (typeof e == "string")
      throw e;
    var n = t === "/", i = !t, a;
    if (n && o.root)
      throw new o.ErrnoError(10);
    if (!n && !i) {
      var s = o.lookupPath(t, { follow_mount: !1 });
      if (t = s.path, a = s.node, o.isMountpoint(a))
        throw new o.ErrnoError(10);
      if (!o.isDir(a.mode))
        throw new o.ErrnoError(54);
    }
    var l = { type: e, opts: r, mountpoint: t, mounts: [] }, d = e.mount(l);
    return d.mount = l, l.root = d, n ? o.root = d : a && (a.mounted = l, a.mount && a.mount.mounts.push(l)), d;
  }, unmount(e) {
    var r = o.lookupPath(e, { follow_mount: !1 });
    if (!o.isMountpoint(r.node))
      throw new o.ErrnoError(28);
    var t = r.node, n = t.mounted, i = o.getMounts(n);
    for (var [a, s] of Object.entries(o.nameTable))
      for (; s; ) {
        var l = s.name_next;
        i.includes(s.mount) && o.destroyNode(s), s = l;
      }
    t.mounted = null;
    var d = t.mount.mounts.indexOf(n);
    u(d !== -1), t.mount.mounts.splice(d, 1);
  }, lookup(e, r) {
    return e.node_ops.lookup(e, r);
  }, mknod(e, r, t) {
    var n = o.lookupPath(e, { parent: !0 }), i = n.node, a = T.basename(e);
    if (!a)
      throw new o.ErrnoError(28);
    if (a === "." || a === "..")
      throw new o.ErrnoError(20);
    var s = o.mayCreate(i, a);
    if (s)
      throw new o.ErrnoError(s);
    if (!i.node_ops.mknod)
      throw new o.ErrnoError(63);
    return i.node_ops.mknod(i, a, r, t);
  }, statfs(e) {
    return o.statfsNode(o.lookupPath(e, { follow: !0 }).node);
  }, statfsStream(e) {
    return o.statfsNode(e.node);
  }, statfsNode(e) {
    var r = { bsize: 4096, frsize: 4096, blocks: 1e6, bfree: 5e5, bavail: 5e5, files: o.nextInode, ffree: o.nextInode - 1, fsid: 42, flags: 2, namelen: 255 };
    return e.node_ops.statfs && Object.assign(r, e.node_ops.statfs(e.mount.opts.root)), r;
  }, create(e, r = 438) {
    return r &= 4095, r |= 32768, o.mknod(e, r, 0);
  }, mkdir(e, r = 511) {
    return r &= 1023, r |= 16384, o.mknod(e, r, 0);
  }, mkdirTree(e, r) {
    var t = e.split("/"), n = "";
    for (var i of t)
      if (i) {
        (n || T.isAbs(e)) && (n += "/"), n += i;
        try {
          o.mkdir(n, r);
        } catch (a) {
          if (a.errno != 20) throw a;
        }
      }
  }, mkdev(e, r, t) {
    return typeof t > "u" && (t = r, r = 438), r |= 8192, o.mknod(e, r, t);
  }, symlink(e, r) {
    if (!Ee.resolve(e))
      throw new o.ErrnoError(44);
    var t = o.lookupPath(r, { parent: !0 }), n = t.node;
    if (!n)
      throw new o.ErrnoError(44);
    var i = T.basename(r), a = o.mayCreate(n, i);
    if (a)
      throw new o.ErrnoError(a);
    if (!n.node_ops.symlink)
      throw new o.ErrnoError(63);
    return n.node_ops.symlink(n, i, e);
  }, rename(e, r) {
    var t = T.dirname(e), n = T.dirname(r), i = T.basename(e), a = T.basename(r), s, l, d;
    if (s = o.lookupPath(e, { parent: !0 }), l = s.node, s = o.lookupPath(r, { parent: !0 }), d = s.node, !l || !d) throw new o.ErrnoError(44);
    if (l.mount !== d.mount)
      throw new o.ErrnoError(75);
    var _ = o.lookupNode(l, i), v = Ee.relative(e, n);
    if (v.charAt(0) !== ".")
      throw new o.ErrnoError(28);
    if (v = Ee.relative(r, t), v.charAt(0) !== ".")
      throw new o.ErrnoError(55);
    var g;
    try {
      g = o.lookupNode(d, a);
    } catch {
    }
    if (_ !== g) {
      var m = o.isDir(_.mode), p = o.mayDelete(l, i, m);
      if (p)
        throw new o.ErrnoError(p);
      if (p = g ? o.mayDelete(d, a, m) : o.mayCreate(d, a), p)
        throw new o.ErrnoError(p);
      if (!l.node_ops.rename)
        throw new o.ErrnoError(63);
      if (o.isMountpoint(_) || g && o.isMountpoint(g))
        throw new o.ErrnoError(10);
      if (d !== l && (p = o.nodePermissions(l, "w"), p))
        throw new o.ErrnoError(p);
      o.hashRemoveNode(_);
      try {
        l.node_ops.rename(_, d, a), _.parent = d;
      } catch (k) {
        throw k;
      } finally {
        o.hashAddNode(_);
      }
    }
  }, rmdir(e) {
    var r = o.lookupPath(e, { parent: !0 }), t = r.node, n = T.basename(e), i = o.lookupNode(t, n), a = o.mayDelete(t, n, !0);
    if (a)
      throw new o.ErrnoError(a);
    if (!t.node_ops.rmdir)
      throw new o.ErrnoError(63);
    if (o.isMountpoint(i))
      throw new o.ErrnoError(10);
    t.node_ops.rmdir(t, n), o.destroyNode(i);
  }, readdir(e) {
    var r = o.lookupPath(e, { follow: !0 }), t = r.node, n = o.checkOpExists(t.node_ops.readdir, 54);
    return n(t);
  }, unlink(e) {
    var r = o.lookupPath(e, { parent: !0 }), t = r.node;
    if (!t)
      throw new o.ErrnoError(44);
    var n = T.basename(e), i = o.lookupNode(t, n), a = o.mayDelete(t, n, !1);
    if (a)
      throw new o.ErrnoError(a);
    if (!t.node_ops.unlink)
      throw new o.ErrnoError(63);
    if (o.isMountpoint(i))
      throw new o.ErrnoError(10);
    t.node_ops.unlink(t, n), o.destroyNode(i);
  }, readlink(e) {
    var r = o.lookupPath(e), t = r.node;
    if (!t)
      throw new o.ErrnoError(44);
    if (!t.node_ops.readlink)
      throw new o.ErrnoError(28);
    return t.node_ops.readlink(t);
  }, stat(e, r) {
    var t = o.lookupPath(e, { follow: !r }), n = t.node, i = o.checkOpExists(n.node_ops.getattr, 63);
    return i(n);
  }, fstat(e) {
    var r = o.getStreamChecked(e), t = r.node, n = r.stream_ops.getattr, i = n ? r : t;
    return n ??= t.node_ops.getattr, o.checkOpExists(n, 63), n(i);
  }, lstat(e) {
    return o.stat(e, !0);
  }, doChmod(e, r, t, n) {
    o.doSetAttr(e, r, { mode: t & 4095 | r.mode & -4096, ctime: Date.now(), dontFollow: n });
  }, chmod(e, r, t) {
    var n;
    if (typeof e == "string") {
      var i = o.lookupPath(e, { follow: !t });
      n = i.node;
    } else
      n = e;
    o.doChmod(null, n, r, t);
  }, lchmod(e, r) {
    o.chmod(e, r, !0);
  }, fchmod(e, r) {
    var t = o.getStreamChecked(e);
    o.doChmod(t, t.node, r, !1);
  }, doChown(e, r, t) {
    o.doSetAttr(e, r, { timestamp: Date.now(), dontFollow: t });
  }, chown(e, r, t, n) {
    var i;
    if (typeof e == "string") {
      var a = o.lookupPath(e, { follow: !n });
      i = a.node;
    } else
      i = e;
    o.doChown(null, i, n);
  }, lchown(e, r, t) {
    o.chown(e, r, t, !0);
  }, fchown(e, r, t) {
    var n = o.getStreamChecked(e);
    o.doChown(n, n.node, !1);
  }, doTruncate(e, r, t) {
    if (o.isDir(r.mode))
      throw new o.ErrnoError(31);
    if (!o.isFile(r.mode))
      throw new o.ErrnoError(28);
    var n = o.nodePermissions(r, "w");
    if (n)
      throw new o.ErrnoError(n);
    o.doSetAttr(e, r, { size: t, timestamp: Date.now() });
  }, truncate(e, r) {
    if (r < 0)
      throw new o.ErrnoError(28);
    var t;
    if (typeof e == "string") {
      var n = o.lookupPath(e, { follow: !0 });
      t = n.node;
    } else
      t = e;
    o.doTruncate(null, t, r);
  }, ftruncate(e, r) {
    var t = o.getStreamChecked(e);
    if (r < 0 || (t.flags & 2097155) === 0)
      throw new o.ErrnoError(28);
    o.doTruncate(t, t.node, r);
  }, utime(e, r, t) {
    var n = o.lookupPath(e, { follow: !0 }), i = n.node, a = o.checkOpExists(i.node_ops.setattr, 63);
    a(i, { atime: r, mtime: t });
  }, open(e, r, t = 438) {
    if (e === "")
      throw new o.ErrnoError(44);
    r = typeof r == "string" ? Tn(r) : r, r & 64 ? t = t & 4095 | 32768 : t = 0;
    var n, i;
    if (typeof e == "object")
      n = e;
    else {
      i = e.endsWith("/");
      var a = o.lookupPath(e, { follow: !(r & 131072), noent_okay: !0 });
      n = a.node, e = a.path;
    }
    var s = !1;
    if (r & 64)
      if (n) {
        if (r & 128)
          throw new o.ErrnoError(20);
      } else {
        if (i)
          throw new o.ErrnoError(31);
        n = o.mknod(e, t | 511, 0), s = !0;
      }
    if (!n)
      throw new o.ErrnoError(44);
    if (o.isChrdev(n.mode) && (r &= -513), r & 65536 && !o.isDir(n.mode))
      throw new o.ErrnoError(54);
    if (!s) {
      var l = o.mayOpen(n, r);
      if (l)
        throw new o.ErrnoError(l);
    }
    r & 512 && !s && o.truncate(n, 0), r &= -131713;
    var d = o.createStream({ node: n, path: o.getPath(n), flags: r, seekable: !0, position: 0, stream_ops: n.stream_ops, ungotten: [], error: !1 });
    return d.stream_ops.open && d.stream_ops.open(d), s && o.chmod(n, t & 511), c.logReadFiles && !(r & 1) && (e in o.readFiles || (o.readFiles[e] = 1)), d;
  }, close(e) {
    if (o.isClosed(e))
      throw new o.ErrnoError(8);
    e.getdents && (e.getdents = null);
    try {
      e.stream_ops.close && e.stream_ops.close(e);
    } catch (r) {
      throw r;
    } finally {
      o.closeStream(e.fd);
    }
    e.fd = null;
  }, isClosed(e) {
    return e.fd === null;
  }, llseek(e, r, t) {
    if (o.isClosed(e))
      throw new o.ErrnoError(8);
    if (!e.seekable || !e.stream_ops.llseek)
      throw new o.ErrnoError(70);
    if (t != 0 && t != 1 && t != 2)
      throw new o.ErrnoError(28);
    return e.position = e.stream_ops.llseek(e, r, t), e.ungotten = [], e.position;
  }, read(e, r, t, n, i) {
    if (u(t >= 0), n < 0 || i < 0)
      throw new o.ErrnoError(28);
    if (o.isClosed(e))
      throw new o.ErrnoError(8);
    if ((e.flags & 2097155) === 1)
      throw new o.ErrnoError(8);
    if (o.isDir(e.node.mode))
      throw new o.ErrnoError(31);
    if (!e.stream_ops.read)
      throw new o.ErrnoError(28);
    var a = typeof i < "u";
    if (!a)
      i = e.position;
    else if (!e.seekable)
      throw new o.ErrnoError(70);
    var s = e.stream_ops.read(e, r, t, n, i);
    return a || (e.position += s), s;
  }, write(e, r, t, n, i, a) {
    if (u(t >= 0), n < 0 || i < 0)
      throw new o.ErrnoError(28);
    if (o.isClosed(e))
      throw new o.ErrnoError(8);
    if ((e.flags & 2097155) === 0)
      throw new o.ErrnoError(8);
    if (o.isDir(e.node.mode))
      throw new o.ErrnoError(31);
    if (!e.stream_ops.write)
      throw new o.ErrnoError(28);
    e.seekable && e.flags & 1024 && o.llseek(e, 0, 2);
    var s = typeof i < "u";
    if (!s)
      i = e.position;
    else if (!e.seekable)
      throw new o.ErrnoError(70);
    var l = e.stream_ops.write(e, r, t, n, i, a);
    return s || (e.position += l), l;
  }, mmap(e, r, t, n, i) {
    if ((n & 2) !== 0 && (i & 2) === 0 && (e.flags & 2097155) !== 2)
      throw new o.ErrnoError(2);
    if ((e.flags & 2097155) === 1)
      throw new o.ErrnoError(2);
    if (!e.stream_ops.mmap)
      throw new o.ErrnoError(43);
    if (!r)
      throw new o.ErrnoError(28);
    return e.stream_ops.mmap(e, r, t, n, i);
  }, msync(e, r, t, n, i) {
    return u(t >= 0), e.stream_ops.msync ? e.stream_ops.msync(e, r, t, n, i) : 0;
  }, ioctl(e, r, t) {
    if (!e.stream_ops.ioctl)
      throw new o.ErrnoError(59);
    return e.stream_ops.ioctl(e, r, t);
  }, readFile(e, r = {}) {
    r.flags = r.flags || 0, r.encoding = r.encoding || "binary", r.encoding !== "utf8" && r.encoding !== "binary" && P(`Invalid encoding type "${r.encoding}"`);
    var t = o.open(e, r.flags), n = o.stat(e), i = n.size, a = new Uint8Array(i);
    return o.read(t, a, 0, i, 0), r.encoding === "utf8" && (a = ge(a)), o.close(t), a;
  }, writeFile(e, r, t = {}) {
    t.flags = t.flags || 577;
    var n = o.open(e, t.flags, t.mode);
    typeof r == "string" && (r = new Uint8Array(dr(r))), ArrayBuffer.isView(r) ? o.write(n, r, 0, r.byteLength, void 0, t.canOwn) : P("Unsupported data type"), o.close(n);
  }, cwd: () => o.currentPath, chdir(e) {
    var r = o.lookupPath(e, { follow: !0 });
    if (r.node === null)
      throw new o.ErrnoError(44);
    if (!o.isDir(r.node.mode))
      throw new o.ErrnoError(54);
    var t = o.nodePermissions(r.node, "x");
    if (t)
      throw new o.ErrnoError(t);
    o.currentPath = r.path;
  }, createDefaultDirectories() {
    o.mkdir("/tmp"), o.mkdir("/home"), o.mkdir("/home/web_user");
  }, createDefaultDevices() {
    o.mkdir("/dev"), o.registerDevice(o.makedev(1, 3), { read: () => 0, write: (n, i, a, s, l) => s, llseek: () => 0 }), o.mkdev("/dev/null", o.makedev(1, 3)), oe.register(o.makedev(5, 0), oe.default_tty_ops), oe.register(o.makedev(6, 0), oe.default_tty1_ops), o.mkdev("/dev/tty", o.makedev(5, 0)), o.mkdev("/dev/tty1", o.makedev(6, 0));
    var e = new Uint8Array(1024), r = 0, t = () => (r === 0 && (sr(e), r = e.byteLength), e[--r]);
    o.createDevice("/dev", "random", t), o.createDevice("/dev", "urandom", t), o.mkdir("/dev/shm"), o.mkdir("/dev/shm/tmp");
  }, createSpecialDirectories() {
    o.mkdir("/proc");
    var e = o.mkdir("/proc/self");
    o.mkdir("/proc/self/fd"), o.mount({ mount() {
      var r = o.createNode(e, "fd", 16895, 73);
      return r.stream_ops = { llseek: E.stream_ops.llseek }, r.node_ops = { lookup(t, n) {
        var i = +n, a = o.getStreamChecked(i), s = { parent: null, mount: { mountpoint: "fake" }, node_ops: { readlink: () => a.path }, id: i + 1 };
        return s.parent = s, s;
      }, readdir() {
        return Array.from(o.streams.entries()).filter(([t, n]) => n).map(([t, n]) => t.toString());
      } }, r;
    } }, {}, "/proc/self/fd");
  }, createStandardStreams(e, r, t) {
    e ? o.createDevice("/dev", "stdin", e) : o.symlink("/dev/tty", "/dev/stdin"), r ? o.createDevice("/dev", "stdout", null, r) : o.symlink("/dev/tty", "/dev/stdout"), t ? o.createDevice("/dev", "stderr", null, t) : o.symlink("/dev/tty1", "/dev/stderr");
    var n = o.open("/dev/stdin", 0), i = o.open("/dev/stdout", 1), a = o.open("/dev/stderr", 1);
    u(n.fd === 0, `invalid handle for stdin (${n.fd})`), u(i.fd === 1, `invalid handle for stdout (${i.fd})`), u(a.fd === 2, `invalid handle for stderr (${a.fd})`);
  }, staticInit() {
    o.nameTable = new Array(4096), o.mount(E, {}, "/"), o.createDefaultDirectories(), o.createDefaultDevices(), o.createSpecialDirectories(), o.filesystems = { MEMFS: E };
  }, init(e, r, t) {
    u(!o.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), o.initialized = !0, e ??= c.stdin, r ??= c.stdout, t ??= c.stderr, o.createStandardStreams(e, r, t);
  }, quit() {
    o.initialized = !1, hr(0);
    for (var e of o.streams)
      e && o.close(e);
  }, findObject(e, r) {
    var t = o.analyzePath(e, r);
    return t.exists ? t.object : null;
  }, analyzePath(e, r) {
    try {
      var t = o.lookupPath(e, { follow: !r });
      e = t.path;
    } catch {
    }
    var n = { isRoot: !1, exists: !1, error: 0, name: null, path: null, object: null, parentExists: !1, parentPath: null, parentObject: null };
    try {
      var t = o.lookupPath(e, { parent: !0 });
      n.parentExists = !0, n.parentPath = t.path, n.parentObject = t.node, n.name = T.basename(e), t = o.lookupPath(e, { follow: !r }), n.exists = !0, n.path = t.path, n.object = t.node, n.name = t.node.name, n.isRoot = t.path === "/";
    } catch (i) {
      n.error = i.errno;
    }
    return n;
  }, createPath(e, r, t, n) {
    e = typeof e == "string" ? e : o.getPath(e);
    for (var i = r.split("/").reverse(); i.length; ) {
      var a = i.pop();
      if (a) {
        var s = T.join2(e, a);
        try {
          o.mkdir(s);
        } catch (l) {
          if (l.errno != 20) throw l;
        }
        e = s;
      }
    }
    return s;
  }, createFile(e, r, t, n, i) {
    var a = T.join2(typeof e == "string" ? e : o.getPath(e), r), s = cr(n, i);
    return o.create(a, s);
  }, createDataFile(e, r, t, n, i, a) {
    var s = r;
    e && (e = typeof e == "string" ? e : o.getPath(e), s = r ? T.join2(e, r) : e);
    var l = cr(n, i), d = o.create(s, l);
    if (t) {
      if (typeof t == "string") {
        for (var _ = new Array(t.length), v = 0, g = t.length; v < g; ++v) _[v] = t.charCodeAt(v);
        t = _;
      }
      o.chmod(d, l | 146);
      var m = o.open(d, 577);
      o.write(m, t, 0, t.length, 0, a), o.close(m), o.chmod(d, l);
    }
  }, createDevice(e, r, t, n) {
    var i = T.join2(typeof e == "string" ? e : o.getPath(e), r), a = cr(!!t, !!n);
    o.createDevice.major ??= 64;
    var s = o.makedev(o.createDevice.major++, 0);
    return o.registerDevice(s, { open(l) {
      l.seekable = !1;
    }, close(l) {
      n?.buffer?.length && n(10);
    }, read(l, d, _, v, g) {
      for (var m = 0, p = 0; p < v; p++) {
        var k;
        try {
          k = t();
        } catch {
          throw new o.ErrnoError(29);
        }
        if (k === void 0 && m === 0)
          throw new o.ErrnoError(6);
        if (k == null) break;
        m++, d[_ + p] = k;
      }
      return m && (l.node.atime = Date.now()), m;
    }, write(l, d, _, v, g) {
      for (var m = 0; m < v; m++)
        try {
          n(d[_ + m]);
        } catch {
          throw new o.ErrnoError(29);
        }
      return v && (l.node.mtime = l.node.ctime = Date.now()), m;
    } }), o.mkdev(i, a, s);
  }, forceLoadFile(e) {
    if (e.isDevice || e.isFolder || e.link || e.contents) return !0;
    if (globalThis.XMLHttpRequest)
      P("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    else
      try {
        e.contents = Ne(e.url);
      } catch {
        throw new o.ErrnoError(29);
      }
  }, createLazyFile(e, r, t, n, i) {
    class a {
      lengthKnown = !1;
      chunks = [];
      get(m) {
        if (!(m > this.length - 1 || m < 0)) {
          var p = m % this.chunkSize, k = m / this.chunkSize | 0;
          return this.getter(k)[p];
        }
      }
      setDataGetter(m) {
        this.getter = m;
      }
      cacheLength() {
        var m = new XMLHttpRequest();
        m.open("HEAD", t, !1), m.send(null), m.status >= 200 && m.status < 300 || m.status === 304 || P("Couldn't load " + t + ". Status: " + m.status);
        var p = Number(m.getResponseHeader("Content-length")), k, S = (k = m.getResponseHeader("Accept-Ranges")) && k === "bytes", D = (k = m.getResponseHeader("Content-Encoding")) && k === "gzip", B = 1024 * 1024;
        S || (B = p);
        var H = (Y, Te) => {
          Y > Te && P("invalid range (" + Y + ", " + Te + ") or no bytes requested!"), Te > p - 1 && P("only " + p + " bytes available! programmer error!");
          var L = new XMLHttpRequest();
          return L.open("GET", t, !1), p !== B && L.setRequestHeader("Range", "bytes=" + Y + "-" + Te), L.responseType = "arraybuffer", L.overrideMimeType && L.overrideMimeType("text/plain; charset=x-user-defined"), L.send(null), L.status >= 200 && L.status < 300 || L.status === 304 || P("Couldn't load " + t + ". Status: " + L.status), L.response !== void 0 ? new Uint8Array(L.response || []) : dr(L.responseText || "");
        }, De = this;
        De.setDataGetter((Y) => {
          var Te = Y * B, L = (Y + 1) * B - 1;
          return L = Math.min(L, p - 1), typeof De.chunks[Y] > "u" && (De.chunks[Y] = H(Te, L)), typeof De.chunks[Y] > "u" && P("doXHR failed!"), De.chunks[Y];
        }), (D || !p) && (B = p = 1, p = this.getter(0).length, B = p, te("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = p, this._chunkSize = B, this.lengthKnown = !0;
      }
      get length() {
        return this.lengthKnown || this.cacheLength(), this._length;
      }
      get chunkSize() {
        return this.lengthKnown || this.cacheLength(), this._chunkSize;
      }
    }
    if (globalThis.XMLHttpRequest) {
      $ || P("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
      var s = new a(), l = { isDevice: !1, contents: s };
    } else
      var l = { isDevice: !1, url: t };
    var d = o.createFile(e, r, l, n, i);
    l.contents ? d.contents = l.contents : l.url && (d.contents = null, d.url = l.url), Object.defineProperties(d, { usedBytes: { get: function() {
      return this.contents.length;
    } } });
    var _ = {};
    for (const [g, m] of Object.entries(d.stream_ops))
      _[g] = (...p) => (o.forceLoadFile(d), m(...p));
    function v(g, m, p, k, S) {
      var D = g.node.contents;
      if (S >= D.length) return 0;
      var B = Math.min(D.length - S, k);
      if (u(B >= 0), D.slice)
        for (var H = 0; H < B; H++)
          m[p + H] = D[S + H];
      else
        for (var H = 0; H < B; H++)
          m[p + H] = D.get(S + H);
      return B;
    }
    return _.read = (g, m, p, k, S) => (o.forceLoadFile(d), v(g, m, p, k, S)), _.mmap = (g, m, p, k, S) => {
      o.forceLoadFile(d);
      var D = et(m);
      if (!D)
        throw new o.ErrnoError(48);
      return v(g, (f(), x), D, m, p), { ptr: D, allocated: !0 };
    }, d.stream_ops = _, d;
  }, absolutePath() {
    P("FS.absolutePath has been removed; use PATH_FS.resolve instead");
  }, createFolder() {
    P("FS.createFolder has been removed; use FS.mkdir instead");
  }, createLink() {
    P("FS.createLink has been removed; use FS.symlink instead");
  }, joinPath() {
    P("FS.joinPath has been removed; use PATH.join instead");
  }, mmapAlloc() {
    P("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
  }, standardizePath() {
    P("FS.standardizePath has been removed; use PATH.normalize instead");
  } }, C = { DEFAULT_POLLMASK: 5, calculateAt(e, r, t) {
    if (T.isAbs(r))
      return r;
    var n;
    if (e === -100)
      n = o.cwd();
    else {
      var i = C.getStreamFromFD(e);
      n = i.path;
    }
    if (r.length == 0) {
      if (!t)
        throw new o.ErrnoError(44);
      return n;
    }
    return n + "/" + r;
  }, writeStat(e, r) {
    (f(), h)[e >> 2] = r.dev, (f(), h)[e + 4 >> 2] = r.mode, (f(), h)[e + 8 >> 2] = r.nlink, (f(), h)[e + 12 >> 2] = r.uid, (f(), h)[e + 16 >> 2] = r.gid, (f(), h)[e + 20 >> 2] = r.rdev, (f(), N)[e + 24 >> 3] = BigInt(r.size), (f(), R)[e + 32 >> 2] = 4096, (f(), R)[e + 36 >> 2] = r.blocks;
    var t = r.atime.getTime(), n = r.mtime.getTime(), i = r.ctime.getTime();
    return (f(), N)[e + 40 >> 3] = BigInt(Math.floor(t / 1e3)), (f(), h)[e + 48 >> 2] = t % 1e3 * 1e3 * 1e3, (f(), N)[e + 56 >> 3] = BigInt(Math.floor(n / 1e3)), (f(), h)[e + 64 >> 2] = n % 1e3 * 1e3 * 1e3, (f(), N)[e + 72 >> 3] = BigInt(Math.floor(i / 1e3)), (f(), h)[e + 80 >> 2] = i % 1e3 * 1e3 * 1e3, (f(), N)[e + 88 >> 3] = BigInt(r.ino), 0;
  }, writeStatFs(e, r) {
    (f(), h)[e + 4 >> 2] = r.bsize, (f(), h)[e + 60 >> 2] = r.bsize, (f(), N)[e + 8 >> 3] = BigInt(r.blocks), (f(), N)[e + 16 >> 3] = BigInt(r.bfree), (f(), N)[e + 24 >> 3] = BigInt(r.bavail), (f(), N)[e + 32 >> 3] = BigInt(r.files), (f(), N)[e + 40 >> 3] = BigInt(r.ffree), (f(), h)[e + 48 >> 2] = r.fsid, (f(), h)[e + 64 >> 2] = r.flags, (f(), h)[e + 56 >> 2] = r.namelen;
  }, doMsync(e, r, t, n, i) {
    if (!o.isFile(r.node.mode))
      throw new o.ErrnoError(43);
    if (n & 2)
      return 0;
    var a = (f(), j).slice(e, e + t);
    o.msync(r, a, i, t, n);
  }, getStreamFromFD(e) {
    var r = o.getStreamChecked(e);
    return r;
  }, varargs: void 0, getStr(e) {
    var r = J(e);
    return r;
  } };
  function ot(e, r, t) {
    if (y) return W(3, 0, 1, e, r, t);
    C.varargs = t;
    try {
      var n = C.getStreamFromFD(e);
      switch (r) {
        case 0: {
          var i = Le();
          if (i < 0)
            return -28;
          for (; o.streams[i]; )
            i++;
          var a;
          return a = o.dupStream(n, i), a.fd;
        }
        case 1:
        case 2:
          return 0;
        case 3:
          return n.flags;
        case 4: {
          var i = Le();
          return n.flags |= i, 0;
        }
        case 12: {
          var i = ye(), s = 0;
          return (f(), ae)[i + s >> 1] = 2, 0;
        }
        case 13:
        case 14:
          return 0;
      }
      return -28;
    } catch (l) {
      if (typeof o > "u" || l.name !== "ErrnoError") throw l;
      return -l.errno;
    }
  }
  function it(e, r) {
    if (y) return W(4, 0, 1, e, r);
    try {
      return C.writeStat(r, o.fstat(e));
    } catch (t) {
      if (typeof o > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function at(e, r, t) {
    if (y) return W(5, 0, 1, e, r, t);
    C.varargs = t;
    try {
      var n = C.getStreamFromFD(e);
      switch (r) {
        case 21509:
          return n.tty ? 0 : -59;
        case 21505: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tcgets) {
            var i = n.tty.ops.ioctl_tcgets(n), a = ye();
            (f(), R)[a >> 2] = i.c_iflag || 0, (f(), R)[a + 4 >> 2] = i.c_oflag || 0, (f(), R)[a + 8 >> 2] = i.c_cflag || 0, (f(), R)[a + 12 >> 2] = i.c_lflag || 0;
            for (var s = 0; s < 32; s++)
              (f(), x)[a + s + 17] = i.c_cc[s] || 0;
            return 0;
          }
          return 0;
        }
        case 21510:
        case 21511:
        case 21512:
          return n.tty ? 0 : -59;
        case 21506:
        case 21507:
        case 21508: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tcsets) {
            for (var a = ye(), l = (f(), R)[a >> 2], d = (f(), R)[a + 4 >> 2], _ = (f(), R)[a + 8 >> 2], v = (f(), R)[a + 12 >> 2], g = [], s = 0; s < 32; s++)
              g.push((f(), x)[a + s + 17]);
            return n.tty.ops.ioctl_tcsets(n.tty, r, { c_iflag: l, c_oflag: d, c_cflag: _, c_lflag: v, c_cc: g });
          }
          return 0;
        }
        case 21519: {
          if (!n.tty) return -59;
          var a = ye();
          return (f(), R)[a >> 2] = 0, 0;
        }
        case 21520:
          return n.tty ? -28 : -59;
        case 21537:
        case 21531: {
          var a = ye();
          return o.ioctl(n, r, a);
        }
        case 21523: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tiocgwinsz) {
            var m = n.tty.ops.ioctl_tiocgwinsz(n.tty), a = ye();
            (f(), ae)[a >> 1] = m[0], (f(), ae)[a + 2 >> 1] = m[1];
          }
          return 0;
        }
        case 21524:
          return n.tty ? 0 : -59;
        case 21515:
          return n.tty ? 0 : -59;
        default:
          return -28;
      }
    } catch (p) {
      if (typeof o > "u" || p.name !== "ErrnoError") throw p;
      return -p.errno;
    }
  }
  function st(e, r) {
    if (y) return W(6, 0, 1, e, r);
    try {
      return e = C.getStr(e), C.writeStat(r, o.lstat(e));
    } catch (t) {
      if (typeof o > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function lt(e, r, t, n) {
    if (y) return W(7, 0, 1, e, r, t, n);
    try {
      r = C.getStr(r);
      var i = n & 256, a = n & 4096;
      return n = n & -6401, u(!n, `unknown flags in __syscall_newfstatat: ${n}`), r = C.calculateAt(e, r, a), C.writeStat(t, i ? o.lstat(r) : o.stat(r));
    } catch (s) {
      if (typeof o > "u" || s.name !== "ErrnoError") throw s;
      return -s.errno;
    }
  }
  function dt(e, r, t, n) {
    if (y) return W(8, 0, 1, e, r, t, n);
    C.varargs = n;
    try {
      r = C.getStr(r), r = C.calculateAt(e, r);
      var i = n ? Le() : 0;
      return o.open(r, t, i).fd;
    } catch (a) {
      if (typeof o > "u" || a.name !== "ErrnoError") throw a;
      return -a.errno;
    }
  }
  function ct(e, r) {
    if (y) return W(9, 0, 1, e, r);
    try {
      return e = C.getStr(e), C.writeStat(r, o.stat(e));
    } catch (t) {
      if (typeof o > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  var Rn = () => P("native code called abort()"), V = (e) => {
    for (var r = ""; ; ) {
      var t = (f(), j)[e++];
      if (!t) return r;
      r += String.fromCharCode(t);
    }
  }, we = {}, be = {}, Ue = {}, Dn = class extends Error {
    constructor(r) {
      super(r), this.name = "BindingError";
    }
  }, G = (e) => {
    throw new Dn(e);
  };
  function Nn(e, r, t = {}) {
    var n = r.name;
    if (e || G(`type "${n}" must have a positive integer typeid pointer`), be.hasOwnProperty(e)) {
      if (t.ignoreDuplicateRegistrations)
        return;
      G(`Cannot register type '${n}' twice`);
    }
    if (be[e] = r, delete Ue[e], we.hasOwnProperty(e)) {
      var i = we[e];
      delete we[e], i.forEach((a) => a());
    }
  }
  function K(e, r, t = {}) {
    return Nn(e, r, t);
  }
  var ut = (e, r, t) => {
    switch (r) {
      case 1:
        return t ? (n) => (f(), x)[n] : (n) => (f(), j)[n];
      case 2:
        return t ? (n) => (f(), ae)[n >> 1] : (n) => (f(), Ce)[n >> 1];
      case 4:
        return t ? (n) => (f(), R)[n >> 2] : (n) => (f(), h)[n >> 2];
      case 8:
        return t ? (n) => (f(), N)[n >> 3] : (n) => (f(), Ir)[n >> 3];
      default:
        throw new TypeError(`invalid integer width (${r}): ${e}`);
    }
  }, $e = (e) => {
    if (e === null)
      return "null";
    var r = typeof e;
    return r === "object" || r === "array" || r === "function" ? e.toString() : "" + e;
  }, ft = (e, r, t, n) => {
    if (r < t || r > n)
      throw new TypeError(`Passing a number "${$e(r)}" from JS side to C/C++ side to an argument of type "${e}", which is outside the valid range [${t}, ${n}]!`);
  }, On = (e, r, t, n, i) => {
    r = V(r);
    const a = n === 0n;
    let s = (l) => l;
    if (a) {
      const l = t * 8;
      s = (d) => BigInt.asUintN(l, d), i = s(i);
    }
    K(e, { name: r, fromWireType: s, toWireType: (l, d) => {
      if (typeof d == "number")
        d = BigInt(d);
      else if (typeof d != "bigint")
        throw new TypeError(`Cannot convert "${$e(d)}" to ${this.name}`);
      return ft(r, d, n, i), d;
    }, readValueFromPointer: ut(r, t, !a), destructorFunction: null });
  }, Wn = (e, r, t, n) => {
    r = V(r), K(e, { name: r, fromWireType: function(i) {
      return !!i;
    }, toWireType: function(i, a) {
      return a ? t : n;
    }, readValueFromPointer: function(i) {
      return this.fromWireType((f(), j)[i]);
    }, destructorFunction: null });
  }, _t = [], Z = [0, 1, , 1, null, 1, !0, 1, !1, 1], xn = (e) => {
    e > 9 && --Z[e + 1] === 0 && (u(Z[e] !== void 0, "Decref for unallocated handle."), Z[e] = void 0, _t.push(e));
  }, mt = { toValue: (e) => (e || G(`Cannot use deleted val. handle = ${e}`), u(e === 2 || Z[e] !== void 0 && e % 2 === 0, `invalid handle: ${e}`), Z[e]), toHandle: (e) => {
    switch (e) {
      case void 0:
        return 2;
      case null:
        return 4;
      case !0:
        return 6;
      case !1:
        return 8;
      default: {
        const r = _t.pop() || Z.length;
        return Z[r] = e, Z[r + 1] = 1, r;
      }
    }
  } };
  function ur(e) {
    return this.fromWireType((f(), h)[e >> 2]);
  }
  var Ln = { name: "emscripten::val", fromWireType: (e) => {
    var r = mt.toValue(e);
    return xn(e), r;
  }, toWireType: (e, r) => mt.toHandle(r), readValueFromPointer: ur, destructorFunction: null }, Un = (e) => K(e, Ln), $n = (e, r) => {
    switch (r) {
      case 4:
        return function(t) {
          return this.fromWireType((f(), Cr)[t >> 2]);
        };
      case 8:
        return function(t) {
          return this.fromWireType((f(), Ie)[t >> 3]);
        };
      default:
        throw new TypeError(`invalid float width (${r}): ${e}`);
    }
  }, Bn = (e, r, t) => {
    r = V(r), K(e, { name: r, fromWireType: (n) => n, toWireType: (n, i) => {
      if (typeof i != "number" && typeof i != "boolean")
        throw new TypeError(`Cannot convert ${$e(i)} to ${this.name}`);
      return i;
    }, readValueFromPointer: $n(r, t), destructorFunction: null });
  }, vt = (e, r) => Object.defineProperty(r, "name", { value: e }), zn = (e) => {
    for (; e.length; ) {
      var r = e.pop(), t = e.pop();
      t(r);
    }
  };
  function ht(e) {
    for (var r = 1; r < e.length; ++r)
      if (e[r] !== null && e[r].destructorFunction === void 0)
        return !0;
    return !1;
  }
  function Hn(e, r, t, n, i) {
    if (e < r || e > t) {
      var a = r == t ? r : `${r} to ${t}`;
      i(`function ${n} called with ${e} arguments, expected ${a}`);
    }
  }
  function jn(e, r, t, n) {
    for (var i = ht(e), a = e.length - 2, s = [], l = ["fn"], d = 0; d < a; ++d)
      s.push(`arg${d}`), l.push(`arg${d}Wired`);
    s = s.join(","), l = l.join(",");
    var _ = `return function (${s}) {
`;
    _ += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, i && (_ += `var destructors = [];
`);
    for (var v = i ? "destructors" : "null", g = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "fromRetWire", "toClassParamWire"], d = 0; d < a; ++d) {
      var m = `toArg${d}Wire`;
      _ += `var arg${d}Wired = ${m}(${v}, arg${d});
`, g.push(m);
    }
    _ += (t || n ? "var rv = " : "") + `invoker(${l});
`;
    var p = t ? "rv" : "";
    if (_ += `function onDone(${p}) {
`, i)
      _ += `runDestructors(destructors);
`;
    else
      for (var d = 2; d < e.length; ++d) {
        var k = d === 1 ? "thisWired" : "arg" + (d - 2) + "Wired";
        e[d].destructorFunction !== null && (_ += `${k}_dtor(${k});
`, g.push(`${k}_dtor`));
      }
    return t && (_ += `var ret = fromRetWire(rv);
return ret;
`), _ += `}
`, _ += "return " + (n ? "rv.then(onDone)" : `onDone(${p})`) + ";", _ += `}
`, g.push("checkArgCount", "minArgs", "maxArgs"), _ = `if (arguments.length !== ${g.length}){ throw new Error(humanName + "Expected ${g.length} closure arguments " + arguments.length + " given."); }
${_}`, new Function(g, _);
  }
  function Vn(e) {
    for (var r = e.length - 2, t = e.length - 1; t >= 2 && e[t].optional; --t)
      r--;
    return r;
  }
  function Gn(e, r, t, n, i, a) {
    var s = r.length;
    s < 2 && G("argTypes array size mismatch! Must at least get return value and 'this' types!");
    for (var l = r[1] !== null && t !== null, d = ht(r), _ = !r[0].isVoid, v = s - 2, g = Vn(r), m = r[0], p = r[1], k = [e, G, n, i, zn, m.fromWireType.bind(m), p?.toWireType.bind(p)], S = 2; S < s; ++S) {
      var D = r[S];
      k.push(D.toWireType.bind(D));
    }
    if (!d)
      for (var S = 2; S < r.length; ++S)
        r[S].destructorFunction !== null && k.push(r[S].destructorFunction);
    k.push(Hn, g, v);
    var H = jn(r, l, _, a)(...k);
    return vt(e, H);
  }
  var qn = (e, r, t) => {
    if (e[r].overloadTable === void 0) {
      var n = e[r];
      e[r] = function(...i) {
        return e[r].overloadTable.hasOwnProperty(i.length) || G(`Function '${t}' called with an invalid number of arguments (${i.length}) - expects one of (${e[r].overloadTable})!`), e[r].overloadTable[i.length].apply(this, i);
      }, e[r].overloadTable = [], e[r].overloadTable[n.argCount] = n;
    }
  }, Kn = (e, r, t) => {
    c.hasOwnProperty(e) ? ((t === void 0 || c[e].overloadTable !== void 0 && c[e].overloadTable[t] !== void 0) && G(`Cannot register public name '${e}' twice`), qn(c, e, e), c[e].overloadTable.hasOwnProperty(t) && G(`Cannot register multiple overloads of a function with the same number of arguments (${t})!`), c[e].overloadTable[t] = r) : (c[e] = r, c[e].argCount = t);
  }, Xn = (e, r) => {
    for (var t = [], n = 0; n < e; n++)
      t.push((f(), h)[r + n * 4 >> 2]);
    return t;
  }, Yn = class extends Error {
    constructor(r) {
      super(r), this.name = "InternalError";
    }
  }, pt = (e) => {
    throw new Yn(e);
  }, Jn = (e, r, t) => {
    c.hasOwnProperty(e) || pt("Replacing nonexistent public symbol"), c[e].overloadTable !== void 0 && t !== void 0 ? c[e].overloadTable[t] = r : (c[e] = r, c[e].argCount = t);
  }, Zn = (e, r, t = !1) => {
    e = V(e);
    function n() {
      var a = Gr(r);
      return t && (a = WebAssembly.promising(a)), a;
    }
    var i = n();
    return typeof i != "function" && G(`unknown function pointer with signature ${e}: ${r}`), i;
  };
  class Qn extends Error {
  }
  var eo = (e) => {
    var r = xt(e), t = V(r);
    return ee(r), t;
  }, ro = (e, r) => {
    var t = [], n = {};
    function i(a) {
      if (!n[a] && !be[a]) {
        if (Ue[a]) {
          Ue[a].forEach(i);
          return;
        }
        t.push(a), n[a] = !0;
      }
    }
    throw r.forEach(i), new Qn(`${e}: ` + t.map(eo).join([", "]));
  }, to = (e, r, t) => {
    e.forEach((l) => Ue[l] = r);
    function n(l) {
      var d = t(l);
      d.length !== e.length && pt("Mismatched type converter count");
      for (var _ = 0; _ < e.length; ++_)
        K(e[_], d[_]);
    }
    var i = new Array(r.length), a = [], s = 0;
    for (let [l, d] of r.entries())
      be.hasOwnProperty(d) ? i[l] = be[d] : (a.push(d), we.hasOwnProperty(d) || (we[d] = []), we[d].push(() => {
        i[l] = be[d], ++s, s === a.length && n(i);
      }));
    a.length === 0 && n(i);
  }, no = (e) => {
    e = e.trim();
    const r = e.indexOf("(");
    return r === -1 ? e : (u(e.endsWith(")"), "Parentheses for argument names should match."), e.slice(0, r));
  }, oo = (e, r, t, n, i, a, s, l) => {
    var d = Xn(r, t);
    e = V(e), e = no(e), i = Zn(n, i, s), Kn(e, function() {
      ro(`Cannot call ${e} due to unbound types`, d);
    }, r - 1), to([], d, (_) => {
      var v = [_[0], null].concat(_.slice(1));
      return Jn(e, Gn(e, v, null, i, a, s), r - 1), [];
    });
  }, io = (e, r, t, n, i) => {
    r = V(r);
    const a = n === 0;
    let s = (d) => d;
    if (a) {
      var l = 32 - 8 * t;
      s = (d) => d << l >>> l, i = s(i);
    }
    K(e, { name: r, fromWireType: s, toWireType: (d, _) => {
      if (typeof _ != "number" && typeof _ != "boolean")
        throw new TypeError(`Cannot convert "${$e(_)}" to ${r}`);
      return ft(r, _, n, i), _;
    }, readValueFromPointer: ut(r, t, n !== 0), destructorFunction: null });
  }, ao = (e, r, t) => {
    var n = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array], i = n[r];
    function a(s) {
      var l = (f(), h)[s >> 2], d = (f(), h)[s + 4 >> 2];
      return new i((f(), x).buffer, d, l);
    }
    t = V(t), K(e, { name: t, fromWireType: a, readValueFromPointer: a }, { ignoreDuplicateRegistrations: !0 });
  }, Q = (e, r, t) => (u(typeof t == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), Zr(e, (f(), j), r, t)), so = (e, r) => {
    r = V(r), K(e, { name: r, fromWireType(t) {
      var n = (f(), h)[t >> 2], i = t + 4, a;
      return a = J(i, n, !0), ee(t), a;
    }, toWireType(t, n) {
      n instanceof ArrayBuffer && (n = new Uint8Array(n));
      var i, a = typeof n == "string";
      a || ArrayBuffer.isView(n) && n.BYTES_PER_ELEMENT == 1 || G("Cannot pass non-string to std::string"), a ? i = ce(n) : i = n.length;
      var s = Ge(4 + i + 1), l = s + 4;
      return (f(), h)[s >> 2] = i, a ? Q(n, l, i + 1) : (f(), j).set(n, l), t !== null && t.push(ee, s), s;
    }, readValueFromPointer: ur, destructorFunction(t) {
      ee(t);
    } });
  }, gt = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, lo = (e, r, t) => {
    u(e % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
    var n = e >> 1, i = Xr((f(), Ce), n, r / 2, t);
    if (i - n > 16 && gt) return gt.decode((f(), Ce).slice(n, i));
    for (var a = "", s = n; s < i; ++s) {
      var l = (f(), Ce)[s];
      a += String.fromCharCode(l);
    }
    return a;
  }, co = (e, r, t) => {
    if (u(r % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), u(typeof t == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 2) return 0;
    t -= 2;
    for (var n = r, i = t < e.length * 2 ? t / 2 : e.length, a = 0; a < i; ++a) {
      var s = e.charCodeAt(a);
      (f(), ae)[r >> 1] = s, r += 2;
    }
    return (f(), ae)[r >> 1] = 0, r - n;
  }, uo = (e) => e.length * 2, fo = (e, r, t) => {
    u(e % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
    for (var n = "", i = e >> 2, a = 0; !(a >= r / 4); a++) {
      var s = (f(), h)[i + a];
      if (!s && !t) break;
      n += String.fromCodePoint(s);
    }
    return n;
  }, _o = (e, r, t) => {
    if (u(r % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), u(typeof t == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 4) return 0;
    for (var n = r, i = n + t - 4, a = 0; a < e.length; ++a) {
      var s = e.codePointAt(a);
      if (s > 65535 && a++, (f(), R)[r >> 2] = s, r += 4, r + 4 > i) break;
    }
    return (f(), R)[r >> 2] = 0, r - n;
  }, mo = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.codePointAt(t);
      n > 65535 && t++, r += 4;
    }
    return r;
  }, vo = (e, r, t) => {
    t = V(t);
    var n, i, a;
    r === 2 ? (n = lo, i = co, a = uo) : (u(r === 4, "only 2-byte and 4-byte strings are currently supported"), n = fo, i = _o, a = mo), K(e, { name: t, fromWireType: (s) => {
      var l = (f(), h)[s >> 2], d = n(s + 4, l * r, !0);
      return ee(s), d;
    }, toWireType: (s, l) => {
      typeof l != "string" && G(`Cannot pass non-string to C++ string type ${t}`);
      var d = a(l), _ = Ge(4 + d + r);
      return (f(), h)[_ >> 2] = d / r, i(l, _ + 4, d + r), s !== null && s.push(ee, _), _;
    }, readValueFromPointer: ur, destructorFunction(s) {
      ee(s);
    } });
  }, ho = (e, r) => {
    r = V(r), K(e, { isVoid: !0, name: r, fromWireType: () => {
    }, toWireType: (t, n) => {
    } });
  }, po = (e) => {
    pr(e, !$, 1, !U, 65536, !1), w.threadInitTLS();
  }, yt = (e) => {
    if (e instanceof Nr || e == "unwind")
      return me;
    Pe(), e instanceof WebAssembly.RuntimeError && Er() <= 0 && b("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), Sr(1, e);
  }, go = () => {
    if (!xe())
      try {
        if (y) {
          Se() && yr(me);
          return;
        }
        or(me);
      } catch (e) {
        yt(e);
      }
  }, Et = (e) => {
    if (_e) {
      b("user callback triggered after runtime exited or application aborted.  Ignoring.");
      return;
    }
    try {
      e(), go();
    } catch (r) {
      yt(r);
    }
  }, fr = (e) => {
    if (Atomics.waitAsync) {
      var r = Atomics.waitAsync((f(), R), e >> 2, e);
      u(r.async), r.value.then(Be);
      var t = e + 128;
      Atomics.store((f(), R), t >> 2, 1);
    }
  }, Be = () => Et(() => {
    var e = Se();
    e && (fr(e), jt());
  }), yo = (e, r) => {
    if (e == r)
      setTimeout(Be);
    else if (y)
      postMessage({ targetThread: e, cmd: "checkMailbox" });
    else {
      var t = w.pthreads[e];
      if (!t) {
        b(`Cannot send message to thread with ID ${e}, unknown thread ID!`);
        return;
      }
      t.postMessage({ cmd: "checkMailbox" });
    }
  }, ze = [], Eo = (e, r, t, n, i) => {
    n /= 2, ze.length = n;
    for (var a = i >> 3, s = 0; s < n; s++)
      (f(), N)[a + 2 * s] ? ze[s] = (f(), N)[a + 2 * s + 1] : ze[s] = (f(), Ie)[a + 2 * s + 1];
    var l = r ? vr[r] : Qo[e];
    u(!(e && r)), u(l.length == n, "Call args mismatch in _emscripten_receive_on_main_thread_js"), w.currentProxiedOperationCallerThread = t;
    var d = l(...ze);
    return w.currentProxiedOperationCallerThread = 0, u(typeof d != "bigint"), d;
  }, wo = (e) => {
    y ? postMessage({ cmd: "cleanupThread", thread: e }) : Wr(e);
  }, bo = (e) => {
  }, So = 9007199254740992, ko = -9007199254740992, _r = (e) => e < ko || e > So ? NaN : Number(e);
  function wt(e, r, t, n, i, a, s) {
    if (y) return W(10, 0, 1, e, r, t, n, i, a, s);
    i = _r(i);
    try {
      u(!isNaN(i));
      var l = C.getStreamFromFD(n), d = o.mmap(l, e, i, r, t), _ = d.ptr;
      return (f(), R)[a >> 2] = d.allocated, (f(), h)[s >> 2] = _, 0;
    } catch (v) {
      if (typeof o > "u" || v.name !== "ErrnoError") throw v;
      return -v.errno;
    }
  }
  function bt(e, r, t, n, i, a) {
    if (y) return W(11, 0, 1, e, r, t, n, i, a);
    a = _r(a);
    try {
      var s = C.getStreamFromFD(i);
      t & 2 && C.doMsync(e, s, r, n, a);
    } catch (l) {
      if (typeof o > "u" || l.name !== "ErrnoError") throw l;
      return -l.errno;
    }
  }
  var To = (e, r, t, n) => {
    var i = (/* @__PURE__ */ new Date()).getFullYear(), a = new Date(i, 0, 1), s = new Date(i, 6, 1), l = a.getTimezoneOffset(), d = s.getTimezoneOffset(), _ = Math.max(l, d);
    (f(), h)[e >> 2] = _ * 60, (f(), R)[r >> 2] = +(l != d);
    var v = (p) => {
      var k = p >= 0 ? "-" : "+", S = Math.abs(p), D = String(Math.floor(S / 60)).padStart(2, "0"), B = String(S % 60).padStart(2, "0");
      return `UTC${k}${D}${B}`;
    }, g = v(l), m = v(d);
    u(g), u(m), u(ce(g) <= 16, `timezone name truncated to fit in TZNAME_MAX (${g})`), u(ce(m) <= 16, `timezone name truncated to fit in TZNAME_MAX (${m})`), d < l ? (Q(g, t, 17), Q(m, n, 17)) : (Q(g, n, 17), Q(m, t, 17));
  }, St = () => performance.timeOrigin + performance.now(), Fo = () => Date.now(), Ao = (e) => e >= 0 && e <= 3;
  function Po(e, r, t) {
    if (!Ao(e))
      return 28;
    var n;
    e === 0 ? n = Fo() : n = St();
    var i = Math.round(n * 1e3 * 1e3);
    return (f(), N)[t >> 3] = BigInt(i), 0;
  }
  var He = [], Co = (e, r) => {
    u(Array.isArray(He)), u(r % 16 == 0), He.length = 0;
    for (var t; t = (f(), j)[e++]; ) {
      var n = String.fromCharCode(t), i = ["d", "f", "i", "p"];
      i.push("j"), u(i.includes(n), `Invalid character ${t}("${n}") in readEmAsmArgs! Use only [${i}], and do not specify "v" for void return argument.`);
      var a = t != 105;
      a &= t != 112, r += a && r % 8 ? 4 : 0, He.push(t == 112 ? (f(), h)[r >> 2] : t == 106 ? (f(), N)[r >> 3] : t == 105 ? (f(), R)[r >> 2] : (f(), Ie)[r >> 3]), r += a ? 8 : 4;
    }
    return He;
  }, Io = (e, r, t) => {
    var n = Co(r, t);
    return u(vr.hasOwnProperty(e), `No EM_ASM constant found at address ${e}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), vr[e](...n);
  }, Mo = (e, r, t) => Io(e, r, t), Ro = () => {
    $ || ne("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
  }, Do = (e, r) => b(J(e, r)), kt = () => {
    pe += 1;
  }, No = () => {
    throw kt(), "unwind";
  }, Tt = () => 2147483648, Oo = () => Tt(), Wo = () => navigator.hardwareConcurrency, ue = {}, Ft = (e) => {
    var r = ce(e) + 1, t = Ge(r);
    return t && Q(e, t, r), t;
  }, je = (e) => {
    var r;
    if (r = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(e))
      return +r[1];
    if (r = /\bwasm-function\[(\d+)\]:(\d+)/.exec(e))
      ne("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
    else if (r = /:(\d+):\d+(?:\)|$)/.exec(e))
      return 2147483648 | +r[1];
    return 0;
  }, At = (e) => {
    for (var r of e) {
      var t = je(r);
      t && (ue[t] = r);
    }
  }, Pt = () => new Error().stack.toString(), xo = () => {
    var e = Pt().split(`
`);
    return e[0] == "Error" && e.shift(), At(e), ue.last_addr = je(e[3]), ue.last_stack = e, ue.last_addr;
  }, Ve = (e) => {
    var r = ue[e];
    if (!r) return 0;
    var t, n;
    if (n = /^\s+at .*\.wasm\.(.*) \(.*\)$/.exec(r))
      t = n[1];
    else if (n = /^\s+at (.*) \(.*\)$/.exec(r))
      t = n[1];
    else if (n = /^(.+?)@/.exec(r))
      t = n[1];
    else
      return 0;
    return ee(Ve.ret ?? 0), Ve.ret = Ft(t), Ve.ret;
  }, Lo = (e) => {
    var r = q.buffer.byteLength, t = (e - r + 65535) / 65536 | 0;
    try {
      return q.grow(t), We(), 1;
    } catch (n) {
      b(`growMemory: Attempted to grow heap from ${r} bytes to ${e} bytes, but got error: ${n}`);
    }
  }, Uo = (e) => {
    var r = (f(), j).length;
    if (e >>>= 0, e <= r)
      return !1;
    var t = Tt();
    if (e > t)
      return b(`Cannot enlarge memory, requested ${e} bytes, but the limit is ${t} bytes!`), !1;
    for (var n = 1; n <= 4; n *= 2) {
      var i = r * (1 + 0.2 / n);
      i = Math.min(i, e + 100663296);
      var a = Math.min(t, Qr(Math.max(e, i), 65536)), s = Lo(a);
      if (s)
        return !0;
    }
    return b(`Failed to grow the heap from ${r} bytes to ${a} bytes, not enough memory!`), !1;
  }, $o = (e, r, t) => {
    var n;
    ue.last_addr == e ? n = ue.last_stack : (n = Pt().split(`
`), n[0] == "Error" && n.shift(), At(n));
    for (var i = 3; n[i] && je(n[i]) != e; )
      ++i;
    for (var a = 0; a < t && n[a + i]; ++a)
      (f(), R)[r + a * 4 >> 2] = je(n[a + i]);
    return a;
  }, mr = {}, Bo = () => br || "./this.program", Re = () => {
    if (!Re.strings) {
      var e = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", r = { USER: "web_user", LOGNAME: "web_user", PATH: "/", PWD: "/", HOME: "/home/web_user", LANG: e, _: Bo() };
      for (var t in mr)
        mr[t] === void 0 ? delete r[t] : r[t] = mr[t];
      var n = [];
      for (var t in r)
        n.push(`${t}=${r[t]}`);
      Re.strings = n;
    }
    return Re.strings;
  };
  function Ct(e, r) {
    if (y) return W(12, 0, 1, e, r);
    var t = 0, n = 0;
    for (var i of Re()) {
      var a = r + t;
      (f(), h)[e + n >> 2] = a, t += Q(i, a, 1 / 0) + 1, n += 4;
    }
    return 0;
  }
  function It(e, r) {
    if (y) return W(13, 0, 1, e, r);
    var t = Re();
    (f(), h)[e >> 2] = t.length;
    var n = 0;
    for (var i of t)
      n += ce(i) + 1;
    return (f(), h)[r >> 2] = n, 0;
  }
  function Mt(e) {
    if (y) return W(14, 0, 1, e);
    try {
      var r = C.getStreamFromFD(e);
      return o.close(r), 0;
    } catch (t) {
      if (typeof o > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var zo = (e, r, t, n) => {
    for (var i = 0, a = 0; a < t; a++) {
      var s = (f(), h)[r >> 2], l = (f(), h)[r + 4 >> 2];
      r += 8;
      var d = o.read(e, (f(), x), s, l, n);
      if (d < 0) return -1;
      if (i += d, d < l) break;
    }
    return i;
  };
  function Rt(e, r, t, n) {
    if (y) return W(15, 0, 1, e, r, t, n);
    try {
      var i = C.getStreamFromFD(e), a = zo(i, r, t);
      return (f(), h)[n >> 2] = a, 0;
    } catch (s) {
      if (typeof o > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function Dt(e, r, t, n) {
    if (y) return W(16, 0, 1, e, r, t, n);
    r = _r(r);
    try {
      if (isNaN(r)) return 61;
      var i = C.getStreamFromFD(e);
      return o.llseek(i, r, t), (f(), N)[n >> 3] = BigInt(i.position), i.getdents && r === 0 && t === 0 && (i.getdents = null), 0;
    } catch (a) {
      if (typeof o > "u" || a.name !== "ErrnoError") throw a;
      return a.errno;
    }
  }
  var Ho = (e, r, t, n) => {
    for (var i = 0, a = 0; a < t; a++) {
      var s = (f(), h)[r >> 2], l = (f(), h)[r + 4 >> 2];
      r += 8;
      var d = o.write(e, (f(), x), s, l, n);
      if (d < 0) return -1;
      if (i += d, d < l)
        break;
    }
    return i;
  };
  function Nt(e, r, t, n) {
    if (y) return W(17, 0, 1, e, r, t, n);
    try {
      var i = C.getStreamFromFD(e), a = Ho(i, r, t);
      return (f(), h)[n >> 2] = a, 0;
    } catch (s) {
      if (typeof o > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function jo(e, r) {
    try {
      return sr((f(), j).subarray(e, e + r)), 0;
    } catch (t) {
      if (typeof o > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var Vo = () => {
    u(pe > 0), pe -= 1;
  }, X = { instrumentWasmImports(e) {
    u("Suspending" in WebAssembly, "JSPI not supported by current environment. Perhaps it needs to be enabled via flags?");
    var r = /^(invoke_.*|__asyncjs__.*)$/;
    for (let [t, n] of Object.entries(e))
      typeof n == "function" && (n.isAsync || r.test(t)) && (e[t] = n = new WebAssembly.Suspending(n));
  }, instrumentFunction(e) {
    var r = (...t) => e(...t);
    return r = vt(`__asyncify_wrapper_${e.name}`, r), r;
  }, instrumentWasmExports(e) {
    var r = /^(solve_model|validate_model|main|__main_argc_argv)$/;
    X.asyncExports = /* @__PURE__ */ new Set();
    var t = {};
    for (let [i, a] of Object.entries(e))
      if (typeof a == "function") {
        r.test(i) && (X.asyncExports.add(a), a = X.makeAsyncFunction(a));
        var n = X.instrumentFunction(a);
        t[i] = n;
      } else
        t[i] = a;
    return t;
  }, asyncExports: null, isAsyncExport(e) {
    return X.asyncExports?.has(e);
  }, handleAsync: async (e) => {
    kt();
    try {
      return await e();
    } finally {
      Vo();
    }
  }, handleSleep: (e) => X.handleAsync(() => new Promise(e)), makeAsyncFunction(e) {
    return WebAssembly.promising(e);
  } }, Go = (e) => {
    var r = c["_" + e];
    return u(r, "Cannot call unknown function " + e + ", make sure it is exported"), r;
  }, qo = (e, r) => {
    u(e.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (f(), x).set(e, r);
  }, Ko = (e) => {
    var r = ce(e) + 1, t = tr(r);
    return Q(e, t, r), t;
  }, Ot = (e, r, t, n, i) => {
    var a = { string: (S) => {
      var D = 0;
      return S != null && S !== 0 && (D = Ko(S)), D;
    }, array: (S) => {
      var D = tr(S.length);
      return qo(S, D), D;
    } };
    function s(S) {
      return r === "string" ? J(S) : r === "boolean" ? !!S : S;
    }
    var l = Go(e), d = [], _ = 0;
    if (u(r !== "array", 'Return type should not be "array".'), n)
      for (var v = 0; v < n.length; v++) {
        var g = a[t[v]];
        g ? (_ === 0 && (_ = Hr()), d[v] = g(n[v])) : d[v] = n[v];
      }
    var m = l(...d);
    function p(S) {
      return _ !== 0 && rr(_), s(S);
    }
    var k = i?.async;
    return k ? m.then(p) : (m = p(m), m);
  }, Xo = (e, r, t, n) => (...i) => Ot(e, r, t, i, n), Yo = (...e) => Ft(...e);
  w.init(), o.createPreloadedFile = Mn, o.preloadFile = nt, o.staticInit(), u(Z.length === 10);
  {
    if (an(), c.noExitRuntime && (ar = c.noExitRuntime), c.preloadPlugins && (tt = c.preloadPlugins), c.print && (te = c.print), c.printErr && (b = c.printErr), c.wasmBinary && (Ae = c.wasmBinary), ei(), c.arguments && c.arguments, c.thisProgram && (br = c.thisProgram), u(typeof c.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), u(typeof c.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), u(typeof c.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), u(typeof c.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), u(typeof c.read > "u", "Module.read option was removed"), u(typeof c.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), u(typeof c.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), u(typeof c.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), u(typeof c.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), u(typeof c.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), u(typeof c.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), c.preInit)
      for (typeof c.preInit == "function" && (c.preInit = [c.preInit]); c.preInit.length > 0; )
        c.preInit.shift()();
    Oe("preInit");
  }
  c.ccall = Ot, c.cwrap = Xo, c.UTF8ToString = J, c.stringToUTF8 = Q, c.allocateUTF8 = Yo;
  var Jo = ["writeI53ToI64", "writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "convertU32PairToI53", "getTempRet0", "setTempRet0", "withStackSave", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "runMainThreadEmAsm", "jstoi_q", "autoResumeAudioContext", "getDynCaller", "dynCall", "asmjsMangle", "HandleAllocator", "addOnInit", "addOnPostCtor", "addOnPreMain", "addOnExit", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "intArrayToString", "stringToAscii", "registerKeyEventCallback", "maybeCStringToJsString", "findEventTarget", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "registerBatteryEventCallback", "setCanvasElementSizeCallingThread", "setCanvasElementSizeMainThread", "setCanvasElementSize", "getCanvasSizeCallingThread", "getCanvasSizeMainThread", "getCanvasElementSize", "getCallstack", "convertPCtoSourceLocation", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "safeSetTimeout", "setImmediateWrapped", "safeRequestAnimationFrame", "clearImmediateWrapped", "registerPostMainLoop", "registerPreMainLoop", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "isLeapYear", "ydayFromDate", "arraySum", "addDays", "getSocketFromFD", "getSocketAddress", "FS_mkdirTree", "_setNetworkCallback", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "webgl_enable_EXT_polygon_offset_clamp", "webgl_enable_EXT_clip_control", "webgl_enable_WEBGL_polygon_mode", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "emscriptenWebGLGetUniform", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "emscripten_webgl_destroy_context_before_on_calling_thread", "registerWebGlEventCallback", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "allocateUTF8OnStack", "demangle", "stackTrace", "getNativeTypeSize", "getFunctionArgsName", "requireRegisteredType", "createJsInvokerSignature", "PureVirtualError", "getBasestPointer", "registerInheritedInstance", "unregisterInheritedInstance", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "enumReadValueFromPointer", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "flushPendingDeletes", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "count_emval_handles", "getStringOrSymbol", "emval_returnValue", "emval_lookupTypes", "emval_addMethodCaller"];
  Jo.forEach(nn);
  var Zo = ["run", "out", "err", "callMain", "abort", "wasmExports", "HEAPF32", "HEAPF64", "HEAP8", "HEAP16", "HEAPU16", "HEAP32", "HEAPU32", "HEAP64", "HEAPU64", "writeStackCookie", "checkStackCookie", "INT53_MAX", "INT53_MIN", "bigintToI53Checked", "stackSave", "stackRestore", "stackAlloc", "createNamedFunction", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "ERRNO_CODES", "strError", "DNS", "Protocols", "Sockets", "timers", "warnOnce", "readEmAsmArgsArray", "readEmAsmArgs", "runEmAsmFunction", "getExecutableName", "handleException", "keepRuntimeAlive", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "wasmMemory", "getUniqueRunDependency", "noExitRuntime", "addRunDependency", "removeRunDependency", "addOnPreRun", "addOnPostRun", "freeTableIndexes", "functionsInTableMap", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "stringToUTF8Array", "lengthBytesUTF8", "intArrayFromString", "AsciiToString", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "stringToUTF8OnStack", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "jsStackTrace", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "checkWasiClock", "doReadv", "doWritev", "initRandomFill", "randomFill", "emSetImmediate", "emClearImmediate_deps", "emClearImmediate", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "requestFullscreen", "requestFullScreen", "setCanvasSize", "getUserMedia", "createContext", "getPreloadedImageData__data", "wget", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_preloadFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS_unlink", "FS_createPath", "FS_createDevice", "FS_readFile", "FS", "FS_root", "FS_mounts", "FS_devices", "FS_streams", "FS_nextInode", "FS_nameTable", "FS_currentPath", "FS_initialized", "FS_ignorePermissions", "FS_filesystems", "FS_syncFSRequests", "FS_readFiles", "FS_lookupPath", "FS_getPath", "FS_hashName", "FS_hashAddNode", "FS_hashRemoveNode", "FS_lookupNode", "FS_createNode", "FS_destroyNode", "FS_isRoot", "FS_isMountpoint", "FS_isFile", "FS_isDir", "FS_isLink", "FS_isChrdev", "FS_isBlkdev", "FS_isFIFO", "FS_isSocket", "FS_flagsToPermissionString", "FS_nodePermissions", "FS_mayLookup", "FS_mayCreate", "FS_mayDelete", "FS_mayOpen", "FS_checkOpExists", "FS_nextfd", "FS_getStreamChecked", "FS_getStream", "FS_createStream", "FS_closeStream", "FS_dupStream", "FS_doSetAttr", "FS_chrdev_stream_ops", "FS_major", "FS_minor", "FS_makedev", "FS_registerDevice", "FS_getDevice", "FS_getMounts", "FS_syncfs", "FS_mount", "FS_unmount", "FS_lookup", "FS_mknod", "FS_statfs", "FS_statfsStream", "FS_statfsNode", "FS_create", "FS_mkdir", "FS_mkdev", "FS_symlink", "FS_rename", "FS_rmdir", "FS_readdir", "FS_readlink", "FS_stat", "FS_fstat", "FS_lstat", "FS_doChmod", "FS_chmod", "FS_lchmod", "FS_fchmod", "FS_doChown", "FS_chown", "FS_lchown", "FS_fchown", "FS_doTruncate", "FS_truncate", "FS_ftruncate", "FS_utime", "FS_open", "FS_close", "FS_isClosed", "FS_llseek", "FS_read", "FS_write", "FS_mmap", "FS_msync", "FS_ioctl", "FS_writeFile", "FS_cwd", "FS_chdir", "FS_createDefaultDirectories", "FS_createDefaultDevices", "FS_createSpecialDirectories", "FS_createStandardStreams", "FS_staticInit", "FS_init", "FS_quit", "FS_findObject", "FS_analyzePath", "FS_createFile", "FS_createDataFile", "FS_forceLoadFile", "FS_createLazyFile", "FS_absolutePath", "FS_createFolder", "FS_createLink", "FS_joinPath", "FS_mmapAlloc", "FS_standardizePath", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "GL", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "runAndAbortIfError", "Asyncify", "Fibers", "SDL", "SDL_gfx", "print", "printErr", "jstoi_s", "PThread", "terminateWorker", "cleanupThread", "registerTLSInit", "spawnThread", "exitOnMainThread", "proxyToMainThread", "proxiedJSCallArgs", "invokeEntryPoint", "checkMailbox", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "getTypeName", "getFunctionName", "heap32VectorToArray", "usesDestructorStack", "checkArgCount", "getRequiredArgCount", "createJsInvoker", "UnboundTypeError", "EmValType", "EmValOptionalType", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "embindRepr", "registeredInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "floatReadValueFromPointer", "assertIntegerRange", "readPointer", "runDestructors", "craftInvokerFunction", "embind__requireFunction", "finalizationRegistry", "detachFinalizer_deps", "deletionQueue", "delayFunction", "emval_freelist", "emval_handles", "emval_symbols", "Emval", "emval_methodCallers"];
  Zo.forEach(Fr);
  var Qo = [nr, jr, Yr, ot, it, at, st, lt, dt, ct, wt, bt, Ct, It, Mt, Rt, Dt, Nt];
  function ei() {
    rn("fetchSettings");
  }
  var vr = { 540932: () => typeof wasmOffsetConverter < "u" };
  function Wt() {
    return typeof wasmOffsetConverter < "u";
  }
  Wt.sig = "i";
  var xt = F("___getTypeName"), Lt = F("__embind_initialize_bindings");
  c._get_cp_model_schema = F("_get_cp_model_schema"), c._get_sat_parameters_schema = F("_get_sat_parameters_schema"), c._solve_model = F("_solve_model");
  var Ge = c._malloc = F("_malloc");
  c._free_buffer = F("_free_buffer");
  var ee = c._free = F("_free");
  c._interrupt_solve = F("_interrupt_solve"), c._validate_model = F("_validate_model");
  var Se = F("_pthread_self"), hr = F("_fflush"), Ut = F("_strerror"), $t = F("_emscripten_builtin_memalign"), pr = F("__emscripten_thread_init"), Bt = F("__emscripten_thread_crashed"), gr = F("_emscripten_stack_get_end"), zt = F("__emscripten_run_js_on_main_thread"), Ht = F("__emscripten_thread_free_data"), yr = F("__emscripten_thread_exit"), jt = F("__emscripten_check_mailbox"), Vt = F("_emscripten_stack_init"), Gt = F("_emscripten_stack_set_limits"), qt = F("__emscripten_stack_restore"), Kt = F("__emscripten_stack_alloc"), Er = F("_emscripten_stack_get_current"), Xt = F("wasmTable");
  function ri(e) {
    u(typeof e.__getTypeName < "u", "missing Wasm export: __getTypeName"), u(typeof e._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), u(typeof e.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), u(typeof e.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), u(typeof e.solve_model < "u", "missing Wasm export: solve_model"), u(typeof e.malloc < "u", "missing Wasm export: malloc"), u(typeof e.free_buffer < "u", "missing Wasm export: free_buffer"), u(typeof e.free < "u", "missing Wasm export: free"), u(typeof e.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), u(typeof e.validate_model < "u", "missing Wasm export: validate_model"), u(typeof e.pthread_self < "u", "missing Wasm export: pthread_self"), u(typeof e.fflush < "u", "missing Wasm export: fflush"), u(typeof e.strerror < "u", "missing Wasm export: strerror"), u(typeof e._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), u(typeof e.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), u(typeof e._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), u(typeof e._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), u(typeof e.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), u(typeof e.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), u(typeof e._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), u(typeof e._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), u(typeof e._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), u(typeof e._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), u(typeof e.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), u(typeof e.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), u(typeof e.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), u(typeof e._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), u(typeof e._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), u(typeof e.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), u(typeof e.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), xt = O("__getTypeName", 1), Lt = O("_embind_initialize_bindings", 0), c._get_cp_model_schema = O("get_cp_model_schema", 0), c._get_sat_parameters_schema = O("get_sat_parameters_schema", 0), c._solve_model = O("solve_model", 5), Ge = c._malloc = O("malloc", 1), c._free_buffer = O("free_buffer", 1), ee = c._free = O("free", 1), c._interrupt_solve = O("interrupt_solve", 0), c._validate_model = O("validate_model", 3), Se = O("pthread_self", 0), hr = O("fflush", 1), Ut = O("strerror", 1), $t = O("emscripten_builtin_memalign", 2), pr = O("_emscripten_thread_init", 6), Bt = O("_emscripten_thread_crashed", 0), gr = e.emscripten_stack_get_end, e.emscripten_stack_get_base, zt = O("_emscripten_run_js_on_main_thread", 5), Ht = O("_emscripten_thread_free_data", 1), yr = O("_emscripten_thread_exit", 1), jt = O("_emscripten_check_mailbox", 0), Vt = e.emscripten_stack_init, Gt = e.emscripten_stack_set_limits, e.emscripten_stack_get_free, qt = e._emscripten_stack_restore, Kt = e._emscripten_stack_alloc, Er = e.emscripten_stack_get_current, Xt = e.__indirect_function_table;
  }
  var ke;
  function ti() {
    ke = { HaveOffsetConverter: Wt, __assert_fail: gn, __cxa_throw: En, __pthread_create_js: Jr, __syscall_fcntl64: ot, __syscall_fstat64: it, __syscall_ioctl: at, __syscall_lstat64: st, __syscall_newfstatat: lt, __syscall_openat: dt, __syscall_stat64: ct, _abort_js: Rn, _embind_register_bigint: On, _embind_register_bool: Wn, _embind_register_emval: Un, _embind_register_float: Bn, _embind_register_function: oo, _embind_register_integer: io, _embind_register_memory_view: ao, _embind_register_std_string: so, _embind_register_std_wstring: vo, _embind_register_void: ho, _emscripten_init_main_thread_js: po, _emscripten_notify_mailbox_postmessage: yo, _emscripten_receive_on_main_thread_js: Eo, _emscripten_thread_cleanup: wo, _emscripten_thread_mailbox_await: fr, _emscripten_thread_set_strongref: bo, _mmap_js: wt, _munmap_js: bt, _tzset_js: To, clock_time_get: Po, emscripten_asm_const_int: Mo, emscripten_check_blocking_allowed: Ro, emscripten_errn: Do, emscripten_exit_with_live_runtime: No, emscripten_get_heap_max: Oo, emscripten_get_now: St, emscripten_num_logical_cores: Wo, emscripten_pc_get_function: Ve, emscripten_resize_heap: Uo, emscripten_stack_snapshot: xo, emscripten_stack_unwind_buffer: $o, environ_get: Ct, environ_sizes_get: It, exit: or, fd_close: Mt, fd_read: Rt, fd_seek: Dt, fd_write: Nt, memory: q, proc_exit: nr, random_get: jo };
  }
  var Yt;
  function ni() {
    u(!y), Vt(), Tr();
  }
  function qe() {
    if (se > 0) {
      Me = qe;
      return;
    }
    if (y) {
      Je?.(c), Mr();
      return;
    }
    if (ni(), sn(), se > 0) {
      Me = qe;
      return;
    }
    async function e() {
      u(!Yt), Yt = !0, c.calledRun = !0, !_e && (Mr(), Je?.(c), c.onRuntimeInitialized?.(), Oe("onRuntimeInitialized"), u(!c._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), ln());
    }
    c.setStatus ? (c.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => c.setStatus(""), 1), e();
    }, 1)) : e(), Pe();
  }
  function oi() {
    var e = te, r = b, t = !1;
    te = b = (d) => {
      t = !0;
    };
    try {
      hr(0);
      for (var n of ["stdout", "stderr"]) {
        var i = o.analyzePath("/dev/" + n);
        if (!i) return;
        var a = i.object, s = a.rdev, l = oe.ttys[s];
        l?.output?.length && (t = !0);
      }
    } catch {
    }
    te = e, b = r, t && ne("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
  }
  var re;
  y || (re = await Dr(), qe()), ve ? A = c : A = new Promise((e, r) => {
    Je = e, Ze = r;
  });
  for (const e of Object.keys(c))
    e in I || Object.defineProperty(I, e, { configurable: !0, get() {
      P(`Access to module property ('${e}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
    } });
  return A;
}
var ii = globalThis.self?.name?.startsWith("em-pthread");
ii && Jt();
async function ai() {
  return await Jt({});
}
const Fe = self;
let M = null;
const si = ai().then((I) => (M = I, Fe.postMessage({ type: "ready" }), I)).catch((I) => {
  throw console.error("[cpsat_worker] cpSatModule init failed:", I), Fe.postMessage({
    type: "error",
    id: 0,
    error: String(I)
  }), I;
}), Zt = (I, A) => new DataView(I, A, 4).getUint32(0, !0), wr = (I) => {
  if (!M || !I?.length)
    return 0;
  const A = M._malloc(I.length);
  return M.HEAPU8.set(I, A), A;
};
async function li(I, A) {
  if (!M)
    throw new Error("Module not initialized.");
  const c = M._malloc(4), U = wr(I), $ = wr(A ?? null);
  let z = 0;
  try {
    z = await M.ccall(
      "solve_model",
      "number",
      ["number", "number", "number", "number", "number"],
      [
        U,
        I.length,
        $,
        A ? A.length : 0,
        c
      ],
      { async: !0 }
    );
  } finally {
    U && M._free(U), $ && M._free($);
  }
  const fe = Zt(M.HEAPU8.buffer, c);
  if (M._free(c), !z || fe === 0)
    return z && M._free_buffer(z), new Uint8Array();
  const y = M.HEAPU8.slice(z, z + fe);
  return M._free_buffer(z), y;
}
async function di(I) {
  if (!M)
    throw new Error("Module not initialized.");
  const A = M._malloc(4), c = wr(I);
  let U = 0;
  try {
    U = M.ccall(
      "validate_model",
      "number",
      ["number", "number", "number"],
      [c, I.length, A]
    );
  } finally {
    c && M._free(c);
  }
  const $ = Zt(M.HEAPU8.buffer, A);
  if (M._free(A), !U || $ === 0)
    return U && M._free_buffer(U), { ok: !0, message: "" };
  const z = M.HEAPU8.slice(U, U + $);
  return M._free_buffer(U), { ok: !1, message: new TextDecoder().decode(z) };
}
Fe.onmessage = async (I) => {
  const A = I.data;
  try {
    if (await si, !M)
      throw new Error("Module not initialized.");
    if (A.type === "validate") {
      const c = await di(A.modelBytes);
      Fe.postMessage({
        type: "validateResult",
        id: A.id,
        ok: c.ok,
        message: c.message
      });
      return;
    }
    if (A.type === "solve") {
      const c = await li(A.modelBytes, A.paramsBytes);
      Fe.postMessage({
        type: "solveResult",
        id: A.id,
        bytes: c
      });
      return;
    }
  } catch (c) {
    console.error("[cpsat_worker] request failed", A?.type, c), Fe.postMessage({
      type: "error",
      id: A?.id ?? 0,
      error: String(c)
    });
  }
};
//# sourceMappingURL=cpsat_worker-p_Fb9hGn.js.map
