async function Qi(ze = {}) {
  var je;
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
    var o = typeof navigator < "u" && navigator.userAgent;
    if (o) {
      var a = o.includes("Safari/") && o.match(/Version\/(\d+\.?\d*\.?\d*)/) ? e(o.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : t;
      if (a < 15e4)
        throw new Error(`This emscripten-generated code requires Safari v${r(15e4)} (detected v${a})`);
      var s = o.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(o.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : t;
      if (s < 114)
        throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${s})`);
      var l = o.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(o.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : t;
      if (l < 85)
        throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${l})`);
    }
  })();
  var f = ze, Ae = !!globalThis.window, Y = !!globalThis.WorkerGlobalScope, He = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", gr = !Ae && !He && !Y, y = Y && self.name?.startsWith("em-pthread");
  y && (c(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0);
  var yr = "./this.program", Er = (e, r) => {
    throw r;
  }, Kt = import.meta.url, Ve = "";
  function Xt(e) {
    return f.locateFile ? f.locateFile(e, Ve) : Ve + e;
  }
  var Ge, Pe;
  if (!gr) if (Ae || Y) {
    try {
      Ve = new URL(".", Kt).href;
    } catch {
    }
    if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    Y && (Pe = (e) => {
      var r = new XMLHttpRequest();
      return r.open("GET", e, !1), r.responseType = "arraybuffer", r.send(null), new Uint8Array(r.response);
    }), Ge = async (e) => {
      c(!wr(e), "readAsync does not work with file:// URLs");
      var r = await fetch(e, { credentials: "same-origin" });
      if (r.ok)
        return r.arrayBuffer();
      throw new Error(r.status + " : " + r.url);
    };
  } else
    throw new Error("environment detection error");
  var J = console.log.bind(console), b = console.error.bind(console);
  c(Ae || Y || He, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), c(!He, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable."), c(!gr, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
  var Ee;
  globalThis.WebAssembly || b("no native wasm support detected");
  var ee, se = !1, le;
  function c(e, r) {
    e || A("Assertion failed" + (r ? ": " + r : ""));
  }
  var wr = (e) => e.startsWith("file://");
  function br() {
    var e = vr();
    c((e & 3) == 0), e == 0 && (e += 4), (u(), h)[e >> 2] = 34821223, (u(), h)[e + 4 >> 2] = 2310721022, (u(), h)[0] = 1668509029;
  }
  function we() {
    if (!se) {
      var e = vr();
      e == 0 && (e += 4);
      var r = (u(), h)[e >> 2], t = (u(), h)[e + 4 >> 2];
      (r != 34821223 || t != 2310721022) && A(`Stack overflow! Stack cookie has been overwritten at ${ie(e)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${ie(t)} ${ie(r)}`), (u(), h)[0] != 1668509029 && A("Runtime error: The application has corrupted its heap memory area (address zero)!");
    }
  }
  function qe(...e) {
    console.warn(...e);
  }
  (() => {
    var e = new Int16Array(1), r = new Int8Array(e.buffer);
    e[0] = 25459, (r[0] !== 115 || r[1] !== 99) && A("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
  })();
  function Ce(e) {
    Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, { configurable: !0, set() {
      A(`Attempt to set \`Module.${e}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
    } });
  }
  function F(e) {
    return () => c(!1, `call to '${e}' via reference taken before Wasm module initialization`);
  }
  function Yt(e) {
    Object.getOwnPropertyDescriptor(f, e) && A(`\`Module.${e}\` was supplied but \`${e}\` not included in INCOMING_MODULE_JS_API`);
  }
  function Jt(e) {
    return e === "FS_createPath" || e === "FS_createDataFile" || e === "FS_createPreloadedFile" || e === "FS_preloadFile" || e === "FS_unlink" || e === "addRunDependency" || e === "FS_createLazyFile" || e === "FS_createDevice" || e === "removeRunDependency";
  }
  function Zt(e) {
    Sr(e);
  }
  function Sr(e) {
    y || Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, { configurable: !0, get() {
      var r = `'${e}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
      Jt(e) && (r += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), A(r);
    } });
  }
  function Qt() {
    function e() {
      var t = 0;
      return de && typeof pe < "u" && (t = pe()), `w:${kr},t:${ie(t)}:`;
    }
    var r = qe;
    qe = (...t) => r(e(), ...t);
  }
  Qt();
  function u() {
    B.buffer != N.buffer && Ie();
  }
  var Ke, Xe, kr = 0, Tr;
  if (y) {
    var Ye = !1;
    self.onunhandledrejection = (r) => {
      throw r.reason || r;
    };
    async function e(r) {
      try {
        var t = r.data, n = t.cmd;
        if (n === "load") {
          kr = t.workerID;
          let o = [];
          self.onmessage = (a) => o.push(a), Tr = () => {
            postMessage({ cmd: "loaded" });
            for (let a of o)
              e(a);
            self.onmessage = e;
          };
          for (const a of t.handlers)
            (!f[a] || f[a].proxy) && (f[a] = (...s) => {
              postMessage({ cmd: "callHandler", handler: a, args: s });
            }, a == "print" && (J = f[a]), a == "printErr" && (b = f[a]));
          B = t.wasmMemory, Ie(), ee = t.wasmModule, Ir(), Be();
        } else if (n === "run") {
          c(t.pthread_ptr), un(t.pthread_ptr), mr(t.pthread_ptr, 0, 0, 1, 0, 0), w.threadInitTLS(), dr(t.pthread_ptr), Ye || (Ot(), Ye = !0);
          try {
            await Hr(t.start_routine, t.arg);
          } catch (o) {
            if (o != "unwind")
              throw o;
          }
        } else t.target === "setimmediate" || (n === "checkMailbox" ? Ye && Oe() : n && (b(`worker: received unknown command ${n}`), b(t)));
      } catch (o) {
        throw b(`worker: onmessage() captured an uncaught exception: ${o}`), o?.stack && b(o.stack), Lt(), o;
      }
    }
    self.onmessage = e;
  }
  var N, L, re, be, C, h, Fr, Se, M, Ar, de = !1;
  function Ie() {
    var e = B.buffer;
    N = new Int8Array(e), re = new Int16Array(e), f.HEAPU8 = L = new Uint8Array(e), be = new Uint16Array(e), C = new Int32Array(e), h = new Uint32Array(e), Fr = new Float32Array(e), Se = new Float64Array(e), M = new BigInt64Array(e), Ar = new BigUint64Array(e);
  }
  function en() {
    if (!y) {
      if (f.wasmMemory)
        B = f.wasmMemory;
      else {
        var e = f.INITIAL_MEMORY || 16777216;
        c(e >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + e + "! (STACK_SIZE=65536)"), B = new WebAssembly.Memory({ initial: e / 65536, maximum: 32768, shared: !0 });
      }
      Ie();
    }
  }
  c(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
  function rn() {
    if (c(!y), f.preRun)
      for (typeof f.preRun == "function" && (f.preRun = [f.preRun]); f.preRun.length; )
        Wr(f.preRun.shift());
    Ce("preRun"), Nr(Or);
  }
  function Pr() {
    if (c(!de), de = !0, y) return Tr();
    we(), !f.noFSInit && !i.initialized && i.init(), X.__wasm_call_ctors(), i.ignorePermissions = !1;
  }
  function tn() {
    if (we(), !y) {
      if (f.postRun)
        for (typeof f.postRun == "function" && (f.postRun = [f.postRun]); f.postRun.length; )
          cn(f.postRun.shift());
      Ce("postRun"), Nr(zr);
    }
  }
  function A(e) {
    f.onAbort?.(e), e = "Aborted(" + e + ")", b(e), se = !0;
    var r = new WebAssembly.RuntimeError(e);
    throw Xe?.(r), r;
  }
  function R(e, r) {
    return (...t) => {
      c(de, `native function \`${e}\` called before runtime initialization`);
      var n = X[e];
      return c(n, `exported native function \`${e}\` not found`), c(t.length <= r, `native function \`${e}\` called with ${t.length} args but expects ${r}`), n(...t);
    };
  }
  var Je;
  function nn() {
    return f.locateFile ? Xt("cp_sat_runtime.wasm") : new URL("/assets/cp_sat_runtime-C04Aywpi.wasm", import.meta.url).href;
  }
  function on(e) {
    if (e == Je && Ee)
      return new Uint8Array(Ee);
    if (Pe)
      return Pe(e);
    throw "both async and sync fetching of the wasm failed";
  }
  async function an(e) {
    if (!Ee)
      try {
        var r = await Ge(e);
        return new Uint8Array(r);
      } catch {
      }
    return on(e);
  }
  async function sn(e, r) {
    try {
      var t = await an(e), n = await WebAssembly.instantiate(t, r);
      return n;
    } catch (o) {
      b(`failed to asynchronously prepare wasm: ${o}`), wr(e) && b(`warning: Loading from a file URI (${e}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), A(o);
    }
  }
  async function ln(e, r, t) {
    if (!e)
      try {
        var n = fetch(r, { credentials: "same-origin" }), o = await WebAssembly.instantiateStreaming(n, t);
        return o;
      } catch (a) {
        b(`wasm streaming compile failed: ${a}`), b("falling back to ArrayBuffer instantiation");
      }
    return sn(r, t);
  }
  function Cr() {
    Yi(), ge.__instrumented || (ge.__instrumented = !0, j.instrumentWasmImports(ge));
    var e = { env: ge, wasi_snapshot_preview1: ge };
    return e;
  }
  async function Ir() {
    function e(l, d) {
      return X = l.exports, X = j.instrumentWasmExports(X), fn(X._emscripten_tls_init), Xi(X), ee = d, X;
    }
    var r = f;
    function t(l) {
      return c(f === r, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), r = null, e(l.instance, l.module);
    }
    var n = Cr();
    if (f.instantiateWasm)
      return new Promise((l, d) => {
        try {
          f.instantiateWasm(n, (_, v) => {
            l(e(_, v));
          });
        } catch (_) {
          b(`Module.instantiateWasm callback failed with error: ${_}`), d(_);
        }
      });
    if (y) {
      c(ee, "wasmModule should have been received via postMessage");
      var o = new WebAssembly.Instance(ee, Cr());
      return e(o, ee);
    }
    Je ??= nn();
    var a = await ln(Ee, Je, n), s = t(a);
    return s;
  }
  class Mr {
    name = "ExitStatus";
    constructor(r) {
      this.message = `Program terminated with exit(${r})`, this.status = r;
    }
  }
  var Rr = (e) => {
    e.terminate(), e.onmessage = (r) => {
      var t = r.data.cmd;
      b(`received "${t}" command from terminated worker: ${e.workerID}`);
    };
  }, Dr = (e) => {
    c(!y, "Internal Error! cleanupThread() can only ever be called from main application thread!"), c(e, "Internal Error! Null pthread_ptr in cleanupThread!");
    var r = w.pthreads[e];
    c(r), w.returnWorkerToPool(r);
  }, Nr = (e) => {
    for (; e.length > 0; )
      e.shift()(f);
  }, Or = [], Wr = (e) => Or.push(e), te = 0, ke = null, ce = {}, ne = null, xr = (e) => {
    if (te--, f.monitorRunDependencies?.(te), c(e, "removeRunDependency requires an ID"), c(ce[e]), delete ce[e], te == 0 && (ne !== null && (clearInterval(ne), ne = null), ke)) {
      var r = ke;
      ke = null, r();
    }
  }, Lr = (e) => {
    te++, f.monitorRunDependencies?.(te), c(e, "addRunDependency requires an ID"), c(!ce[e]), ce[e] = 1, ne === null && globalThis.setInterval && (ne = setInterval(() => {
      if (se) {
        clearInterval(ne), ne = null;
        return;
      }
      var r = !1;
      for (var t in ce)
        r || (r = !0, b("still waiting on run dependencies:")), b(`dependency: ${t}`);
      r && b("(end of list)");
    }, 1e4));
  }, Ur = (e) => {
    c(!y, "Internal Error! spawnThread() can only ever be called from main application thread!"), c(e.pthread_ptr, "Internal error, no pthread ptr!");
    var r = w.getNewWorker();
    if (!r)
      return 6;
    c(!r.pthread_ptr, "Internal error!"), w.runningWorkers.push(r), w.pthreads[e.pthread_ptr] = r, r.pthread_ptr = e.pthread_ptr;
    var t = { cmd: "run", start_routine: e.startRoutine, arg: e.arg, pthread_ptr: e.pthread_ptr };
    return r.postMessage(t, e.transferList), 0;
  }, ue = 0, Me = () => nr || ue > 0, $r = () => pr(), Ze = (e) => Ht(e), Qe = (e) => Vt(e), D = (e, r, t, ...n) => {
    for (var o = n.length * 2, a = $r(), s = Qe(o * 8), l = s >> 3, d = 0; d < n.length; d++) {
      var _ = n[d];
      typeof _ == "bigint" ? ((u(), M)[l + 2 * d] = 1n, (u(), M)[l + 2 * d + 1] = _) : ((u(), M)[l + 2 * d] = 0n, (u(), Se)[l + 2 * d + 1] = _);
    }
    var v = Ut(e, r, o, s, t);
    return Ze(a), v;
  };
  function er(e) {
    if (y) return D(0, 0, 1, e);
    le = e, Me() || (w.terminateAllThreads(), f.onExit?.(e), se = !0), Er(e, new Mr(e));
  }
  function Br(e) {
    if (y) return D(1, 0, 0, e);
    rr(e);
  }
  var dn = (e, r) => {
    if (le = e, Zi(), y)
      throw c(!r), Br(e), "unwind";
    if (Me() && !r) {
      var t = `program exited (with status: ${e}), but keepRuntimeAlive() is set (counter=${ue}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
      Xe?.(t), b(t);
    }
    er(e);
  }, rr = dn, ie = (e) => (c(typeof e == "number", `ptrToString expects a number, got ${typeof e}`), e >>>= 0, "0x" + e.toString(16).padStart(8, "0")), w = { unusedWorkers: [], runningWorkers: [], tlsInitFunctions: [], pthreads: {}, nextWorkerID: 1, init() {
    y || w.initMainThread();
  }, initMainThread() {
    for (var e = Math.max(1, Math.min(8, typeof navigator < "u" && navigator.hardwareConcurrency || 8)); e--; )
      w.allocateUnusedWorker();
    Wr(async () => {
      var r = w.loadWasmModuleToAllWorkers();
      Lr("loading-workers"), await r, xr("loading-workers");
    });
  }, terminateAllThreads: () => {
    c(!y, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
    for (var e of w.runningWorkers)
      Rr(e);
    for (var e of w.unusedWorkers)
      Rr(e);
    w.unusedWorkers = [], w.runningWorkers = [], w.pthreads = {};
  }, returnWorkerToPool: (e) => {
    var r = e.pthread_ptr;
    delete w.pthreads[r], w.unusedWorkers.push(e), w.runningWorkers.splice(w.runningWorkers.indexOf(e), 1), e.pthread_ptr = 0, $t(r);
  }, threadInitTLS() {
    w.tlsInitFunctions.forEach((e) => e());
  }, loadWasmModuleToWorker: (e) => new Promise((r) => {
    e.onmessage = (a) => {
      var s = a.data, l = s.cmd;
      if (s.targetThread && s.targetThread != pe()) {
        var d = w.pthreads[s.targetThread];
        d ? d.postMessage(s, s.transferList) : b(`Internal error! Worker sent a message "${l}" to target pthread ${s.targetThread}, but that thread no longer exists!`);
        return;
      }
      l === "checkMailbox" ? Oe() : l === "spawnThread" ? Ur(s) : l === "cleanupThread" ? pt(() => Dr(s.thread)) : l === "loaded" ? (e.loaded = !0, r(e)) : s.target === "setimmediate" ? e.postMessage(s) : l === "callHandler" ? f[s.handler](...s.args) : l && b(`worker sent an unknown command ${l}`);
    }, e.onerror = (a) => {
      var s = "worker sent an error!";
      throw e.pthread_ptr && (s = `Pthread ${ie(e.pthread_ptr)} sent an error!`), b(`${s} ${a.filename}:${a.lineno}: ${a.message}`), a;
    }, c(B instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), c(ee instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
    var t = [], n = ["onExit", "onAbort", "print", "printErr"];
    for (var o of n)
      f.propertyIsEnumerable(o) && t.push(o);
    e.postMessage({ cmd: "load", handlers: t, wasmMemory: B, wasmModule: ee, workerID: e.workerID });
  }), async loadWasmModuleToAllWorkers() {
    return y ? void 0 : Promise.all(w.unusedWorkers.map(w.loadWasmModuleToWorker));
  }, allocateUnusedWorker() {
    var e;
    if (f.mainScriptUrlOrBlob) {
      var r = f.mainScriptUrlOrBlob;
      typeof r != "string" && (r = URL.createObjectURL(r)), e = new Worker(r, { type: "module", name: "em-pthread-" + w.nextWorkerID });
    } else e = new Worker(self.location.href, { type: "module", name: "em-pthread-" + w.nextWorkerID });
    e.workerID = w.nextWorkerID++, w.unusedWorkers.push(e);
  }, getNewWorker() {
    return w.unusedWorkers.length == 0 && (b("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.\nIf you want to throw an explicit error instead of the risk of deadlocking in those cases, use setting `-sPTHREAD_POOL_SIZE_STRICT=2`."), w.allocateUnusedWorker(), w.loadWasmModuleToWorker(w.unusedWorkers[0])), w.unusedWorkers.pop();
  } }, zr = [], cn = (e) => zr.push(e);
  function un(e) {
    var r = (u(), h)[e + 52 >> 2], t = (u(), h)[e + 56 >> 2], n = r - t;
    c(r != 0), c(n != 0), c(r > n, "stackHigh must be higher then stackLow"), jt(r, n), Ze(r), br();
  }
  var tr = [], jr = (e) => {
    var r = tr[e];
    return r || (tr[e] = r = Gt.get(e), j.isAsyncExport(r) && (tr[e] = r = j.makeAsyncFunction(r))), r;
  }, Hr = async (e, r) => {
    ue = 0, nr = 0;
    var t = WebAssembly.promising(jr(e))(r);
    we();
    function n(o) {
      if (Me()) {
        le = o;
        return;
      }
      hr(o);
    }
    t = await t, n(t);
  };
  Hr.isAsync = !0;
  var nr = !0, fn = (e) => w.tlsInitFunctions.push(e), Z = (e) => {
    Z.shown ||= {}, Z.shown[e] || (Z.shown[e] = 1, b(e));
  }, B, Vr = globalThis.TextDecoder && new TextDecoder(), Gr = (e, r, t, n) => {
    var o = r + t;
    if (n) return o;
    for (; e[r] && !(r >= o); ) ++r;
    return r;
  }, fe = (e, r = 0, t, n) => {
    var o = Gr(e, r, t, n);
    if (o - r > 16 && e.buffer && Vr)
      return Vr.decode(e.buffer instanceof ArrayBuffer ? e.subarray(r, o) : e.slice(r, o));
    for (var a = ""; r < o; ) {
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
      if ((s & 240) == 224 ? s = (s & 15) << 12 | l << 6 | d : ((s & 248) != 240 && Z("Invalid UTF-8 leading byte " + ie(s) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), s = (s & 7) << 18 | l << 12 | d << 6 | e[r++] & 63), s < 65536)
        a += String.fromCharCode(s);
      else {
        var _ = s - 65536;
        a += String.fromCharCode(55296 | _ >> 10, 56320 | _ & 1023);
      }
    }
    return a;
  }, V = (e, r, t) => (c(typeof e == "number", `UTF8ToString expects a number (got ${typeof e})`), e ? fe((u(), L), e, r, t) : ""), _n = (e, r, t, n) => A(`Assertion failed: ${V(e)}, at: ` + [r ? V(r) : "unknown filename", t, n ? V(n) : "unknown function"]);
  class mn {
    constructor(r) {
      this.excPtr = r, this.ptr = r - 24;
    }
    set_type(r) {
      (u(), h)[this.ptr + 4 >> 2] = r;
    }
    get_type() {
      return (u(), h)[this.ptr + 4 >> 2];
    }
    set_destructor(r) {
      (u(), h)[this.ptr + 8 >> 2] = r;
    }
    get_destructor() {
      return (u(), h)[this.ptr + 8 >> 2];
    }
    set_caught(r) {
      r = r ? 1 : 0, (u(), N)[this.ptr + 12] = r;
    }
    get_caught() {
      return (u(), N)[this.ptr + 12] != 0;
    }
    set_rethrown(r) {
      r = r ? 1 : 0, (u(), N)[this.ptr + 13] = r;
    }
    get_rethrown() {
      return (u(), N)[this.ptr + 13] != 0;
    }
    init(r, t) {
      this.set_adjusted_ptr(0), this.set_type(r), this.set_destructor(t);
    }
    set_adjusted_ptr(r) {
      (u(), h)[this.ptr + 16 >> 2] = r;
    }
    get_adjusted_ptr() {
      return (u(), h)[this.ptr + 16 >> 2];
    }
  }
  var vn = (e, r, t) => {
    var n = new mn(e);
    n.init(r, t), c(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
  };
  function qr(e, r, t, n) {
    return y ? D(2, 0, 1, e, r, t, n) : Kr(e, r, t, n);
  }
  var hn = () => !!globalThis.SharedArrayBuffer, Kr = (e, r, t, n) => {
    if (!hn())
      return qe("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
    var o = [], a = 0;
    if (y && (o.length === 0 || a))
      return qr(e, r, t, n);
    var s = { startRoutine: t, pthread_ptr: e, arg: n, transferList: o };
    return y ? (s.cmd = "spawnThread", postMessage(s, o), 0) : Ur(s);
  }, Re = () => {
    c(P.varargs != null);
    var e = (u(), C)[+P.varargs >> 2];
    return P.varargs += 4, e;
  }, _e = Re, T = { isAbs: (e) => e.charAt(0) === "/", splitPath: (e) => {
    var r = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
    return r.exec(e).slice(1);
  }, normalizeArray: (e, r) => {
    for (var t = 0, n = e.length - 1; n >= 0; n--) {
      var o = e[n];
      o === "." ? e.splice(n, 1) : o === ".." ? (e.splice(n, 1), t++) : t && (e.splice(n, 1), t--);
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
  }, basename: (e) => e && e.match(/([^\/]+|\/)\/*$/)[1], join: (...e) => T.normalize(e.join("/")), join2: (e, r) => T.normalize(e + "/" + r) }, pn = () => (e) => e.set(crypto.getRandomValues(new Uint8Array(e.byteLength))), ir = (e) => {
    (ir = pn())(e);
  }, me = { resolve: (...e) => {
    for (var r = "", t = !1, n = e.length - 1; n >= -1 && !t; n--) {
      var o = n >= 0 ? e[n] : i.cwd();
      if (typeof o != "string")
        throw new TypeError("Arguments to path.resolve must be strings");
      if (!o)
        return "";
      r = o + "/" + r, t = T.isAbs(o);
    }
    return r = T.normalizeArray(r.split("/").filter((a) => !!a), !t).join("/"), (t ? "/" : "") + r || ".";
  }, relative: (e, r) => {
    e = me.resolve(e).slice(1), r = me.resolve(r).slice(1);
    function t(_) {
      for (var v = 0; v < _.length && _[v] === ""; v++)
        ;
      for (var g = _.length - 1; g >= 0 && _[g] === ""; g--)
        ;
      return v > g ? [] : _.slice(v, g - v + 1);
    }
    for (var n = t(e.split("/")), o = t(r.split("/")), a = Math.min(n.length, o.length), s = a, l = 0; l < a; l++)
      if (n[l] !== o[l]) {
        s = l;
        break;
      }
    for (var d = [], l = s; l < n.length; l++)
      d.push("..");
    return d = d.concat(o.slice(s)), d.join("/");
  } }, or = [], oe = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.charCodeAt(t);
      n <= 127 ? r++ : n <= 2047 ? r += 2 : n >= 55296 && n <= 57343 ? (r += 4, ++t) : r += 3;
    }
    return r;
  }, Xr = (e, r, t, n) => {
    if (c(typeof e == "string", `stringToUTF8Array expects a string (got ${typeof e})`), !(n > 0)) return 0;
    for (var o = t, a = t + n - 1, s = 0; s < e.length; ++s) {
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
        l > 1114111 && Z("Invalid Unicode code point " + ie(l) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), r[t++] = 240 | l >> 18, r[t++] = 128 | l >> 12 & 63, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63, s++;
      }
    }
    return r[t] = 0, t - o;
  }, ar = (e, r, t) => {
    var n = oe(e) + 1, o = new Array(n), a = Xr(e, o, 0, o.length);
    return o.length = a, o;
  }, gn = () => {
    if (!or.length) {
      var e = null;
      if (globalThis.window?.prompt && (e = window.prompt("Input: "), e !== null && (e += `
`)), !e)
        return null;
      or = ar(e);
    }
    return or.shift();
  }, Q = { ttys: [], init() {
  }, shutdown() {
  }, register(e, r) {
    Q.ttys[e] = { input: [], output: [], ops: r }, i.registerDevice(e, Q.stream_ops);
  }, stream_ops: { open(e) {
    var r = Q.ttys[e.node.rdev];
    if (!r)
      throw new i.ErrnoError(43);
    e.tty = r, e.seekable = !1;
  }, close(e) {
    e.tty.ops.fsync(e.tty);
  }, fsync(e) {
    e.tty.ops.fsync(e.tty);
  }, read(e, r, t, n, o) {
    if (!e.tty || !e.tty.ops.get_char)
      throw new i.ErrnoError(60);
    for (var a = 0, s = 0; s < n; s++) {
      var l;
      try {
        l = e.tty.ops.get_char(e.tty);
      } catch {
        throw new i.ErrnoError(29);
      }
      if (l === void 0 && a === 0)
        throw new i.ErrnoError(6);
      if (l == null) break;
      a++, r[t + s] = l;
    }
    return a && (e.node.atime = Date.now()), a;
  }, write(e, r, t, n, o) {
    if (!e.tty || !e.tty.ops.put_char)
      throw new i.ErrnoError(60);
    try {
      for (var a = 0; a < n; a++)
        e.tty.ops.put_char(e.tty, r[t + a]);
    } catch {
      throw new i.ErrnoError(29);
    }
    return n && (e.node.mtime = e.node.ctime = Date.now()), a;
  } }, default_tty_ops: { get_char(e) {
    return gn();
  }, put_char(e, r) {
    r === null || r === 10 ? (J(fe(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (J(fe(e.output)), e.output = []);
  }, ioctl_tcgets(e) {
    return { c_iflag: 25856, c_oflag: 5, c_cflag: 191, c_lflag: 35387, c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  }, ioctl_tcsets(e, r, t) {
    return 0;
  }, ioctl_tiocgwinsz(e) {
    return [24, 80];
  } }, default_tty1_ops: { put_char(e, r) {
    r === null || r === 10 ? (b(fe(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (b(fe(e.output)), e.output = []);
  } } }, yn = (e, r) => (u(), L).fill(0, e, e + r), Yr = (e, r) => (c(r, "alignment argument is required"), Math.ceil(e / r) * r), Jr = (e) => {
    e = Yr(e, 65536);
    var r = xt(65536, e);
    return r && yn(r, e), r;
  }, E = { ops_table: null, mount(e) {
    return E.createNode(null, "/", 16895, 0);
  }, createNode(e, r, t, n) {
    if (i.isBlkdev(t) || i.isFIFO(t))
      throw new i.ErrnoError(63);
    E.ops_table ||= { dir: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr, lookup: E.node_ops.lookup, mknod: E.node_ops.mknod, rename: E.node_ops.rename, unlink: E.node_ops.unlink, rmdir: E.node_ops.rmdir, readdir: E.node_ops.readdir, symlink: E.node_ops.symlink }, stream: { llseek: E.stream_ops.llseek } }, file: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr }, stream: { llseek: E.stream_ops.llseek, read: E.stream_ops.read, write: E.stream_ops.write, mmap: E.stream_ops.mmap, msync: E.stream_ops.msync } }, link: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr, readlink: E.node_ops.readlink }, stream: {} }, chrdev: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr }, stream: i.chrdev_stream_ops } };
    var o = i.createNode(e, r, t, n);
    return i.isDir(o.mode) ? (o.node_ops = E.ops_table.dir.node, o.stream_ops = E.ops_table.dir.stream, o.contents = {}) : i.isFile(o.mode) ? (o.node_ops = E.ops_table.file.node, o.stream_ops = E.ops_table.file.stream, o.usedBytes = 0, o.contents = null) : i.isLink(o.mode) ? (o.node_ops = E.ops_table.link.node, o.stream_ops = E.ops_table.link.stream) : i.isChrdev(o.mode) && (o.node_ops = E.ops_table.chrdev.node, o.stream_ops = E.ops_table.chrdev.stream), o.atime = o.mtime = o.ctime = Date.now(), e && (e.contents[r] = o, e.atime = e.mtime = e.ctime = o.atime), o;
  }, getFileDataAsTypedArray(e) {
    return e.contents ? e.contents.subarray ? e.contents.subarray(0, e.usedBytes) : new Uint8Array(e.contents) : new Uint8Array(0);
  }, expandFileStorage(e, r) {
    var t = e.contents ? e.contents.length : 0;
    if (!(t >= r)) {
      var n = 1024 * 1024;
      r = Math.max(r, t * (t < n ? 2 : 1.125) >>> 0), t != 0 && (r = Math.max(r, 256));
      var o = e.contents;
      e.contents = new Uint8Array(r), e.usedBytes > 0 && e.contents.set(o.subarray(0, e.usedBytes), 0);
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
    return r.dev = i.isChrdev(e.mode) ? e.id : 1, r.ino = e.id, r.mode = e.mode, r.nlink = 1, r.uid = 0, r.gid = 0, r.rdev = e.rdev, i.isDir(e.mode) ? r.size = 4096 : i.isFile(e.mode) ? r.size = e.usedBytes : i.isLink(e.mode) ? r.size = e.link.length : r.size = 0, r.atime = new Date(e.atime), r.mtime = new Date(e.mtime), r.ctime = new Date(e.ctime), r.blksize = 4096, r.blocks = Math.ceil(r.size / r.blksize), r;
  }, setattr(e, r) {
    for (const t of ["mode", "atime", "mtime", "ctime"])
      r[t] != null && (e[t] = r[t]);
    r.size !== void 0 && E.resizeFileStorage(e, r.size);
  }, lookup(e, r) {
    throw new i.ErrnoError(44);
  }, mknod(e, r, t, n) {
    return E.createNode(e, r, t, n);
  }, rename(e, r, t) {
    var n;
    try {
      n = i.lookupNode(r, t);
    } catch {
    }
    if (n) {
      if (i.isDir(e.mode))
        for (var o in n.contents)
          throw new i.ErrnoError(55);
      i.hashRemoveNode(n);
    }
    delete e.parent.contents[e.name], r.contents[t] = e, e.name = t, r.ctime = r.mtime = e.parent.ctime = e.parent.mtime = Date.now();
  }, unlink(e, r) {
    delete e.contents[r], e.ctime = e.mtime = Date.now();
  }, rmdir(e, r) {
    var t = i.lookupNode(e, r);
    for (var n in t.contents)
      throw new i.ErrnoError(55);
    delete e.contents[r], e.ctime = e.mtime = Date.now();
  }, readdir(e) {
    return [".", "..", ...Object.keys(e.contents)];
  }, symlink(e, r, t) {
    var n = E.createNode(e, r, 41471, 0);
    return n.link = t, n;
  }, readlink(e) {
    if (!i.isLink(e.mode))
      throw new i.ErrnoError(28);
    return e.link;
  } }, stream_ops: { read(e, r, t, n, o) {
    var a = e.node.contents;
    if (o >= e.node.usedBytes) return 0;
    var s = Math.min(e.node.usedBytes - o, n);
    if (c(s >= 0), s > 8 && a.subarray)
      r.set(a.subarray(o, o + s), t);
    else
      for (var l = 0; l < s; l++) r[t + l] = a[o + l];
    return s;
  }, write(e, r, t, n, o, a) {
    if (c(!(r instanceof ArrayBuffer)), r.buffer === (u(), N).buffer && (a = !1), !n) return 0;
    var s = e.node;
    if (s.mtime = s.ctime = Date.now(), r.subarray && (!s.contents || s.contents.subarray)) {
      if (a)
        return c(o === 0, "canOwn must imply no weird position inside the file"), s.contents = r.subarray(t, t + n), s.usedBytes = n, n;
      if (s.usedBytes === 0 && o === 0)
        return s.contents = r.slice(t, t + n), s.usedBytes = n, n;
      if (o + n <= s.usedBytes)
        return s.contents.set(r.subarray(t, t + n), o), n;
    }
    if (E.expandFileStorage(s, o + n), s.contents.subarray && r.subarray)
      s.contents.set(r.subarray(t, t + n), o);
    else
      for (var l = 0; l < n; l++)
        s.contents[o + l] = r[t + l];
    return s.usedBytes = Math.max(s.usedBytes, o + n), n;
  }, llseek(e, r, t) {
    var n = r;
    if (t === 1 ? n += e.position : t === 2 && i.isFile(e.node.mode) && (n += e.node.usedBytes), n < 0)
      throw new i.ErrnoError(28);
    return n;
  }, mmap(e, r, t, n, o) {
    if (!i.isFile(e.node.mode))
      throw new i.ErrnoError(43);
    var a, s, l = e.node.contents;
    if (!(o & 2) && l && l.buffer === (u(), N).buffer)
      s = !1, a = l.byteOffset;
    else {
      if (s = !0, a = Jr(r), !a)
        throw new i.ErrnoError(48);
      l && ((t > 0 || t + r < l.length) && (l.subarray ? l = l.subarray(t, t + r) : l = Array.prototype.slice.call(l, t, t + r)), (u(), N).set(l, a));
    }
    return { ptr: a, allocated: s };
  }, msync(e, r, t, n, o) {
    return E.stream_ops.write(e, r, 0, n, t, !1), 0;
  } } }, En = (e) => {
    var r = { r: 0, "r+": 2, w: 577, "w+": 578, a: 1089, "a+": 1090 }, t = r[e];
    if (typeof t > "u")
      throw new Error(`Unknown file open mode: ${e}`);
    return t;
  }, sr = (e, r) => {
    var t = 0;
    return e && (t |= 365), r && (t |= 146), t;
  }, wn = (e) => V(Wt(e)), Zr = { EPERM: 63, ENOENT: 44, ESRCH: 71, EINTR: 27, EIO: 29, ENXIO: 60, E2BIG: 1, ENOEXEC: 45, EBADF: 8, ECHILD: 12, EAGAIN: 6, EWOULDBLOCK: 6, ENOMEM: 48, EACCES: 2, EFAULT: 21, ENOTBLK: 105, EBUSY: 10, EEXIST: 20, EXDEV: 75, ENODEV: 43, ENOTDIR: 54, EISDIR: 31, EINVAL: 28, ENFILE: 41, EMFILE: 33, ENOTTY: 59, ETXTBSY: 74, EFBIG: 22, ENOSPC: 51, ESPIPE: 70, EROFS: 69, EMLINK: 34, EPIPE: 64, EDOM: 18, ERANGE: 68, ENOMSG: 49, EIDRM: 24, ECHRNG: 106, EL2NSYNC: 156, EL3HLT: 107, EL3RST: 108, ELNRNG: 109, EUNATCH: 110, ENOCSI: 111, EL2HLT: 112, EDEADLK: 16, ENOLCK: 46, EBADE: 113, EBADR: 114, EXFULL: 115, ENOANO: 104, EBADRQC: 103, EBADSLT: 102, EDEADLOCK: 16, EBFONT: 101, ENOSTR: 100, ENODATA: 116, ETIME: 117, ENOSR: 118, ENONET: 119, ENOPKG: 120, EREMOTE: 121, ENOLINK: 47, EADV: 122, ESRMNT: 123, ECOMM: 124, EPROTO: 65, EMULTIHOP: 36, EDOTDOT: 125, EBADMSG: 9, ENOTUNIQ: 126, EBADFD: 127, EREMCHG: 128, ELIBACC: 129, ELIBBAD: 130, ELIBSCN: 131, ELIBMAX: 132, ELIBEXEC: 133, ENOSYS: 52, ENOTEMPTY: 55, ENAMETOOLONG: 37, ELOOP: 32, EOPNOTSUPP: 138, EPFNOSUPPORT: 139, ECONNRESET: 15, ENOBUFS: 42, EAFNOSUPPORT: 5, EPROTOTYPE: 67, ENOTSOCK: 57, ENOPROTOOPT: 50, ESHUTDOWN: 140, ECONNREFUSED: 14, EADDRINUSE: 3, ECONNABORTED: 13, ENETUNREACH: 40, ENETDOWN: 38, ETIMEDOUT: 73, EHOSTDOWN: 142, EHOSTUNREACH: 23, EINPROGRESS: 26, EALREADY: 7, EDESTADDRREQ: 17, EMSGSIZE: 35, EPROTONOSUPPORT: 66, ESOCKTNOSUPPORT: 137, EADDRNOTAVAIL: 4, ENETRESET: 39, EISCONN: 30, ENOTCONN: 53, ETOOMANYREFS: 141, EUSERS: 136, EDQUOT: 19, ESTALE: 72, ENOTSUP: 138, ENOMEDIUM: 148, EILSEQ: 25, EOVERFLOW: 61, ECANCELED: 11, ENOTRECOVERABLE: 56, EOWNERDEAD: 62, ESTRPIPE: 135 }, bn = async (e) => {
    var r = await Ge(e);
    return c(r, `Loading data file "${e}" failed (no arrayBuffer).`), new Uint8Array(r);
  }, Sn = (...e) => i.createDataFile(...e), kn = (e) => {
    for (var r = e; ; ) {
      if (!ce[e]) return e;
      e = r + Math.random();
    }
  }, Qr = [], Tn = async (e, r) => {
    typeof Browser < "u" && Browser.init();
    for (var t of Qr)
      if (t.canHandle(r))
        return c(t.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), t.handle(e, r);
    return e;
  }, et = async (e, r, t, n, o, a, s, l) => {
    var d = r ? me.resolve(T.join2(e, r)) : e, _ = kn(`cp ${d}`);
    Lr(_);
    try {
      var v = t;
      typeof t == "string" && (v = await bn(t)), v = await Tn(v, d), l?.(), a || Sn(e, r, v, n, o, s);
    } finally {
      xr(_);
    }
  }, Fn = (e, r, t, n, o, a, s, l, d, _) => {
    et(e, r, t, n, o, l, d, _).then(a).catch(s);
  }, i = { root: null, mounts: [], devices: {}, streams: [], nextInode: 1, nameTable: null, currentPath: "/", initialized: !1, ignorePermissions: !0, filesystems: null, syncFSRequests: 0, readFiles: {}, ErrnoError: class extends Error {
    name = "ErrnoError";
    constructor(e) {
      super(de ? wn(e) : ""), this.errno = e;
      for (var r in Zr)
        if (Zr[r] === e) {
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
      e || (e = this), this.parent = e, this.mount = e.mount, this.id = i.nextInode++, this.name = r, this.mode = t, this.rdev = n, this.atime = this.mtime = this.ctime = Date.now();
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
      return i.isDir(this.mode);
    }
    get isDevice() {
      return i.isChrdev(this.mode);
    }
  }, lookupPath(e, r = {}) {
    if (!e)
      throw new i.ErrnoError(44);
    r.follow_mount ??= !0, T.isAbs(e) || (e = i.cwd() + "/" + e);
    e: for (var t = 0; t < 40; t++) {
      for (var n = e.split("/").filter((_) => !!_), o = i.root, a = "/", s = 0; s < n.length; s++) {
        var l = s === n.length - 1;
        if (l && r.parent)
          break;
        if (n[s] !== ".") {
          if (n[s] === "..") {
            if (a = T.dirname(a), i.isRoot(o)) {
              e = a + "/" + n.slice(s + 1).join("/"), t--;
              continue e;
            } else
              o = o.parent;
            continue;
          }
          a = T.join2(a, n[s]);
          try {
            o = i.lookupNode(o, n[s]);
          } catch (_) {
            if (_?.errno === 44 && l && r.noent_okay)
              return { path: a };
            throw _;
          }
          if (i.isMountpoint(o) && (!l || r.follow_mount) && (o = o.mounted.root), i.isLink(o.mode) && (!l || r.follow)) {
            if (!o.node_ops.readlink)
              throw new i.ErrnoError(52);
            var d = o.node_ops.readlink(o);
            T.isAbs(d) || (d = T.dirname(a) + "/" + d), e = d + "/" + n.slice(s + 1).join("/");
            continue e;
          }
        }
      }
      return { path: a, node: o };
    }
    throw new i.ErrnoError(32);
  }, getPath(e) {
    for (var r; ; ) {
      if (i.isRoot(e)) {
        var t = e.mount.mountpoint;
        return r ? t[t.length - 1] !== "/" ? `${t}/${r}` : t + r : t;
      }
      r = r ? `${e.name}/${r}` : e.name, e = e.parent;
    }
  }, hashName(e, r) {
    for (var t = 0, n = 0; n < r.length; n++)
      t = (t << 5) - t + r.charCodeAt(n) | 0;
    return (e + t >>> 0) % i.nameTable.length;
  }, hashAddNode(e) {
    var r = i.hashName(e.parent.id, e.name);
    e.name_next = i.nameTable[r], i.nameTable[r] = e;
  }, hashRemoveNode(e) {
    var r = i.hashName(e.parent.id, e.name);
    if (i.nameTable[r] === e)
      i.nameTable[r] = e.name_next;
    else
      for (var t = i.nameTable[r]; t; ) {
        if (t.name_next === e) {
          t.name_next = e.name_next;
          break;
        }
        t = t.name_next;
      }
  }, lookupNode(e, r) {
    var t = i.mayLookup(e);
    if (t)
      throw new i.ErrnoError(t);
    for (var n = i.hashName(e.id, r), o = i.nameTable[n]; o; o = o.name_next) {
      var a = o.name;
      if (o.parent.id === e.id && a === r)
        return o;
    }
    return i.lookup(e, r);
  }, createNode(e, r, t, n) {
    c(typeof e == "object");
    var o = new i.FSNode(e, r, t, n);
    return i.hashAddNode(o), o;
  }, destroyNode(e) {
    i.hashRemoveNode(e);
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
    return i.ignorePermissions ? 0 : r.includes("r") && !(e.mode & 292) || r.includes("w") && !(e.mode & 146) || r.includes("x") && !(e.mode & 73) ? 2 : 0;
  }, mayLookup(e) {
    if (!i.isDir(e.mode)) return 54;
    var r = i.nodePermissions(e, "x");
    return r || (e.node_ops.lookup ? 0 : 2);
  }, mayCreate(e, r) {
    if (!i.isDir(e.mode))
      return 54;
    try {
      var t = i.lookupNode(e, r);
      return 20;
    } catch {
    }
    return i.nodePermissions(e, "wx");
  }, mayDelete(e, r, t) {
    var n;
    try {
      n = i.lookupNode(e, r);
    } catch (a) {
      return a.errno;
    }
    var o = i.nodePermissions(e, "wx");
    if (o)
      return o;
    if (t) {
      if (!i.isDir(n.mode))
        return 54;
      if (i.isRoot(n) || i.getPath(n) === i.cwd())
        return 10;
    } else if (i.isDir(n.mode))
      return 31;
    return 0;
  }, mayOpen(e, r) {
    return e ? i.isLink(e.mode) ? 32 : i.isDir(e.mode) && (i.flagsToPermissionString(r) !== "r" || r & 576) ? 31 : i.nodePermissions(e, i.flagsToPermissionString(r)) : 44;
  }, checkOpExists(e, r) {
    if (!e)
      throw new i.ErrnoError(r);
    return e;
  }, MAX_OPEN_FDS: 4096, nextfd() {
    for (var e = 0; e <= i.MAX_OPEN_FDS; e++)
      if (!i.streams[e])
        return e;
    throw new i.ErrnoError(33);
  }, getStreamChecked(e) {
    var r = i.getStream(e);
    if (!r)
      throw new i.ErrnoError(8);
    return r;
  }, getStream: (e) => i.streams[e], createStream(e, r = -1) {
    return c(r >= -1), e = Object.assign(new i.FSStream(), e), r == -1 && (r = i.nextfd()), e.fd = r, i.streams[r] = e, e;
  }, closeStream(e) {
    i.streams[e] = null;
  }, dupStream(e, r = -1) {
    var t = i.createStream(e, r);
    return t.stream_ops?.dup?.(t), t;
  }, doSetAttr(e, r, t) {
    var n = e?.stream_ops.setattr, o = n ? e : r;
    n ??= r.node_ops.setattr, i.checkOpExists(n, 63), n(o, t);
  }, chrdev_stream_ops: { open(e) {
    var r = i.getDevice(e.node.rdev);
    e.stream_ops = r.stream_ops, e.stream_ops.open?.(e);
  }, llseek() {
    throw new i.ErrnoError(70);
  } }, major: (e) => e >> 8, minor: (e) => e & 255, makedev: (e, r) => e << 8 | r, registerDevice(e, r) {
    i.devices[e] = { stream_ops: r };
  }, getDevice: (e) => i.devices[e], getMounts(e) {
    for (var r = [], t = [e]; t.length; ) {
      var n = t.pop();
      r.push(n), t.push(...n.mounts);
    }
    return r;
  }, syncfs(e, r) {
    typeof e == "function" && (r = e, e = !1), i.syncFSRequests++, i.syncFSRequests > 1 && b(`warning: ${i.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
    var t = i.getMounts(i.root.mount), n = 0;
    function o(l) {
      return c(i.syncFSRequests > 0), i.syncFSRequests--, r(l);
    }
    function a(l) {
      if (l)
        return a.errored ? void 0 : (a.errored = !0, o(l));
      ++n >= t.length && o(null);
    }
    for (var s of t)
      s.type.syncfs ? s.type.syncfs(s, e, a) : a(null);
  }, mount(e, r, t) {
    if (typeof e == "string")
      throw e;
    var n = t === "/", o = !t, a;
    if (n && i.root)
      throw new i.ErrnoError(10);
    if (!n && !o) {
      var s = i.lookupPath(t, { follow_mount: !1 });
      if (t = s.path, a = s.node, i.isMountpoint(a))
        throw new i.ErrnoError(10);
      if (!i.isDir(a.mode))
        throw new i.ErrnoError(54);
    }
    var l = { type: e, opts: r, mountpoint: t, mounts: [] }, d = e.mount(l);
    return d.mount = l, l.root = d, n ? i.root = d : a && (a.mounted = l, a.mount && a.mount.mounts.push(l)), d;
  }, unmount(e) {
    var r = i.lookupPath(e, { follow_mount: !1 });
    if (!i.isMountpoint(r.node))
      throw new i.ErrnoError(28);
    var t = r.node, n = t.mounted, o = i.getMounts(n);
    for (var [a, s] of Object.entries(i.nameTable))
      for (; s; ) {
        var l = s.name_next;
        o.includes(s.mount) && i.destroyNode(s), s = l;
      }
    t.mounted = null;
    var d = t.mount.mounts.indexOf(n);
    c(d !== -1), t.mount.mounts.splice(d, 1);
  }, lookup(e, r) {
    return e.node_ops.lookup(e, r);
  }, mknod(e, r, t) {
    var n = i.lookupPath(e, { parent: !0 }), o = n.node, a = T.basename(e);
    if (!a)
      throw new i.ErrnoError(28);
    if (a === "." || a === "..")
      throw new i.ErrnoError(20);
    var s = i.mayCreate(o, a);
    if (s)
      throw new i.ErrnoError(s);
    if (!o.node_ops.mknod)
      throw new i.ErrnoError(63);
    return o.node_ops.mknod(o, a, r, t);
  }, statfs(e) {
    return i.statfsNode(i.lookupPath(e, { follow: !0 }).node);
  }, statfsStream(e) {
    return i.statfsNode(e.node);
  }, statfsNode(e) {
    var r = { bsize: 4096, frsize: 4096, blocks: 1e6, bfree: 5e5, bavail: 5e5, files: i.nextInode, ffree: i.nextInode - 1, fsid: 42, flags: 2, namelen: 255 };
    return e.node_ops.statfs && Object.assign(r, e.node_ops.statfs(e.mount.opts.root)), r;
  }, create(e, r = 438) {
    return r &= 4095, r |= 32768, i.mknod(e, r, 0);
  }, mkdir(e, r = 511) {
    return r &= 1023, r |= 16384, i.mknod(e, r, 0);
  }, mkdirTree(e, r) {
    var t = e.split("/"), n = "";
    for (var o of t)
      if (o) {
        (n || T.isAbs(e)) && (n += "/"), n += o;
        try {
          i.mkdir(n, r);
        } catch (a) {
          if (a.errno != 20) throw a;
        }
      }
  }, mkdev(e, r, t) {
    return typeof t > "u" && (t = r, r = 438), r |= 8192, i.mknod(e, r, t);
  }, symlink(e, r) {
    if (!me.resolve(e))
      throw new i.ErrnoError(44);
    var t = i.lookupPath(r, { parent: !0 }), n = t.node;
    if (!n)
      throw new i.ErrnoError(44);
    var o = T.basename(r), a = i.mayCreate(n, o);
    if (a)
      throw new i.ErrnoError(a);
    if (!n.node_ops.symlink)
      throw new i.ErrnoError(63);
    return n.node_ops.symlink(n, o, e);
  }, rename(e, r) {
    var t = T.dirname(e), n = T.dirname(r), o = T.basename(e), a = T.basename(r), s, l, d;
    if (s = i.lookupPath(e, { parent: !0 }), l = s.node, s = i.lookupPath(r, { parent: !0 }), d = s.node, !l || !d) throw new i.ErrnoError(44);
    if (l.mount !== d.mount)
      throw new i.ErrnoError(75);
    var _ = i.lookupNode(l, o), v = me.relative(e, n);
    if (v.charAt(0) !== ".")
      throw new i.ErrnoError(28);
    if (v = me.relative(r, t), v.charAt(0) !== ".")
      throw new i.ErrnoError(55);
    var g;
    try {
      g = i.lookupNode(d, a);
    } catch {
    }
    if (_ !== g) {
      var m = i.isDir(_.mode), p = i.mayDelete(l, o, m);
      if (p)
        throw new i.ErrnoError(p);
      if (p = g ? i.mayDelete(d, a, m) : i.mayCreate(d, a), p)
        throw new i.ErrnoError(p);
      if (!l.node_ops.rename)
        throw new i.ErrnoError(63);
      if (i.isMountpoint(_) || g && i.isMountpoint(g))
        throw new i.ErrnoError(10);
      if (d !== l && (p = i.nodePermissions(l, "w"), p))
        throw new i.ErrnoError(p);
      i.hashRemoveNode(_);
      try {
        l.node_ops.rename(_, d, a), _.parent = d;
      } catch (k) {
        throw k;
      } finally {
        i.hashAddNode(_);
      }
    }
  }, rmdir(e) {
    var r = i.lookupPath(e, { parent: !0 }), t = r.node, n = T.basename(e), o = i.lookupNode(t, n), a = i.mayDelete(t, n, !0);
    if (a)
      throw new i.ErrnoError(a);
    if (!t.node_ops.rmdir)
      throw new i.ErrnoError(63);
    if (i.isMountpoint(o))
      throw new i.ErrnoError(10);
    t.node_ops.rmdir(t, n), i.destroyNode(o);
  }, readdir(e) {
    var r = i.lookupPath(e, { follow: !0 }), t = r.node, n = i.checkOpExists(t.node_ops.readdir, 54);
    return n(t);
  }, unlink(e) {
    var r = i.lookupPath(e, { parent: !0 }), t = r.node;
    if (!t)
      throw new i.ErrnoError(44);
    var n = T.basename(e), o = i.lookupNode(t, n), a = i.mayDelete(t, n, !1);
    if (a)
      throw new i.ErrnoError(a);
    if (!t.node_ops.unlink)
      throw new i.ErrnoError(63);
    if (i.isMountpoint(o))
      throw new i.ErrnoError(10);
    t.node_ops.unlink(t, n), i.destroyNode(o);
  }, readlink(e) {
    var r = i.lookupPath(e), t = r.node;
    if (!t)
      throw new i.ErrnoError(44);
    if (!t.node_ops.readlink)
      throw new i.ErrnoError(28);
    return t.node_ops.readlink(t);
  }, stat(e, r) {
    var t = i.lookupPath(e, { follow: !r }), n = t.node, o = i.checkOpExists(n.node_ops.getattr, 63);
    return o(n);
  }, fstat(e) {
    var r = i.getStreamChecked(e), t = r.node, n = r.stream_ops.getattr, o = n ? r : t;
    return n ??= t.node_ops.getattr, i.checkOpExists(n, 63), n(o);
  }, lstat(e) {
    return i.stat(e, !0);
  }, doChmod(e, r, t, n) {
    i.doSetAttr(e, r, { mode: t & 4095 | r.mode & -4096, ctime: Date.now(), dontFollow: n });
  }, chmod(e, r, t) {
    var n;
    if (typeof e == "string") {
      var o = i.lookupPath(e, { follow: !t });
      n = o.node;
    } else
      n = e;
    i.doChmod(null, n, r, t);
  }, lchmod(e, r) {
    i.chmod(e, r, !0);
  }, fchmod(e, r) {
    var t = i.getStreamChecked(e);
    i.doChmod(t, t.node, r, !1);
  }, doChown(e, r, t) {
    i.doSetAttr(e, r, { timestamp: Date.now(), dontFollow: t });
  }, chown(e, r, t, n) {
    var o;
    if (typeof e == "string") {
      var a = i.lookupPath(e, { follow: !n });
      o = a.node;
    } else
      o = e;
    i.doChown(null, o, n);
  }, lchown(e, r, t) {
    i.chown(e, r, t, !0);
  }, fchown(e, r, t) {
    var n = i.getStreamChecked(e);
    i.doChown(n, n.node, !1);
  }, doTruncate(e, r, t) {
    if (i.isDir(r.mode))
      throw new i.ErrnoError(31);
    if (!i.isFile(r.mode))
      throw new i.ErrnoError(28);
    var n = i.nodePermissions(r, "w");
    if (n)
      throw new i.ErrnoError(n);
    i.doSetAttr(e, r, { size: t, timestamp: Date.now() });
  }, truncate(e, r) {
    if (r < 0)
      throw new i.ErrnoError(28);
    var t;
    if (typeof e == "string") {
      var n = i.lookupPath(e, { follow: !0 });
      t = n.node;
    } else
      t = e;
    i.doTruncate(null, t, r);
  }, ftruncate(e, r) {
    var t = i.getStreamChecked(e);
    if (r < 0 || (t.flags & 2097155) === 0)
      throw new i.ErrnoError(28);
    i.doTruncate(t, t.node, r);
  }, utime(e, r, t) {
    var n = i.lookupPath(e, { follow: !0 }), o = n.node, a = i.checkOpExists(o.node_ops.setattr, 63);
    a(o, { atime: r, mtime: t });
  }, open(e, r, t = 438) {
    if (e === "")
      throw new i.ErrnoError(44);
    r = typeof r == "string" ? En(r) : r, r & 64 ? t = t & 4095 | 32768 : t = 0;
    var n, o;
    if (typeof e == "object")
      n = e;
    else {
      o = e.endsWith("/");
      var a = i.lookupPath(e, { follow: !(r & 131072), noent_okay: !0 });
      n = a.node, e = a.path;
    }
    var s = !1;
    if (r & 64)
      if (n) {
        if (r & 128)
          throw new i.ErrnoError(20);
      } else {
        if (o)
          throw new i.ErrnoError(31);
        n = i.mknod(e, t | 511, 0), s = !0;
      }
    if (!n)
      throw new i.ErrnoError(44);
    if (i.isChrdev(n.mode) && (r &= -513), r & 65536 && !i.isDir(n.mode))
      throw new i.ErrnoError(54);
    if (!s) {
      var l = i.mayOpen(n, r);
      if (l)
        throw new i.ErrnoError(l);
    }
    r & 512 && !s && i.truncate(n, 0), r &= -131713;
    var d = i.createStream({ node: n, path: i.getPath(n), flags: r, seekable: !0, position: 0, stream_ops: n.stream_ops, ungotten: [], error: !1 });
    return d.stream_ops.open && d.stream_ops.open(d), s && i.chmod(n, t & 511), f.logReadFiles && !(r & 1) && (e in i.readFiles || (i.readFiles[e] = 1)), d;
  }, close(e) {
    if (i.isClosed(e))
      throw new i.ErrnoError(8);
    e.getdents && (e.getdents = null);
    try {
      e.stream_ops.close && e.stream_ops.close(e);
    } catch (r) {
      throw r;
    } finally {
      i.closeStream(e.fd);
    }
    e.fd = null;
  }, isClosed(e) {
    return e.fd === null;
  }, llseek(e, r, t) {
    if (i.isClosed(e))
      throw new i.ErrnoError(8);
    if (!e.seekable || !e.stream_ops.llseek)
      throw new i.ErrnoError(70);
    if (t != 0 && t != 1 && t != 2)
      throw new i.ErrnoError(28);
    return e.position = e.stream_ops.llseek(e, r, t), e.ungotten = [], e.position;
  }, read(e, r, t, n, o) {
    if (c(t >= 0), n < 0 || o < 0)
      throw new i.ErrnoError(28);
    if (i.isClosed(e))
      throw new i.ErrnoError(8);
    if ((e.flags & 2097155) === 1)
      throw new i.ErrnoError(8);
    if (i.isDir(e.node.mode))
      throw new i.ErrnoError(31);
    if (!e.stream_ops.read)
      throw new i.ErrnoError(28);
    var a = typeof o < "u";
    if (!a)
      o = e.position;
    else if (!e.seekable)
      throw new i.ErrnoError(70);
    var s = e.stream_ops.read(e, r, t, n, o);
    return a || (e.position += s), s;
  }, write(e, r, t, n, o, a) {
    if (c(t >= 0), n < 0 || o < 0)
      throw new i.ErrnoError(28);
    if (i.isClosed(e))
      throw new i.ErrnoError(8);
    if ((e.flags & 2097155) === 0)
      throw new i.ErrnoError(8);
    if (i.isDir(e.node.mode))
      throw new i.ErrnoError(31);
    if (!e.stream_ops.write)
      throw new i.ErrnoError(28);
    e.seekable && e.flags & 1024 && i.llseek(e, 0, 2);
    var s = typeof o < "u";
    if (!s)
      o = e.position;
    else if (!e.seekable)
      throw new i.ErrnoError(70);
    var l = e.stream_ops.write(e, r, t, n, o, a);
    return s || (e.position += l), l;
  }, mmap(e, r, t, n, o) {
    if ((n & 2) !== 0 && (o & 2) === 0 && (e.flags & 2097155) !== 2)
      throw new i.ErrnoError(2);
    if ((e.flags & 2097155) === 1)
      throw new i.ErrnoError(2);
    if (!e.stream_ops.mmap)
      throw new i.ErrnoError(43);
    if (!r)
      throw new i.ErrnoError(28);
    return e.stream_ops.mmap(e, r, t, n, o);
  }, msync(e, r, t, n, o) {
    return c(t >= 0), e.stream_ops.msync ? e.stream_ops.msync(e, r, t, n, o) : 0;
  }, ioctl(e, r, t) {
    if (!e.stream_ops.ioctl)
      throw new i.ErrnoError(59);
    return e.stream_ops.ioctl(e, r, t);
  }, readFile(e, r = {}) {
    r.flags = r.flags || 0, r.encoding = r.encoding || "binary", r.encoding !== "utf8" && r.encoding !== "binary" && A(`Invalid encoding type "${r.encoding}"`);
    var t = i.open(e, r.flags), n = i.stat(e), o = n.size, a = new Uint8Array(o);
    return i.read(t, a, 0, o, 0), r.encoding === "utf8" && (a = fe(a)), i.close(t), a;
  }, writeFile(e, r, t = {}) {
    t.flags = t.flags || 577;
    var n = i.open(e, t.flags, t.mode);
    typeof r == "string" && (r = new Uint8Array(ar(r))), ArrayBuffer.isView(r) ? i.write(n, r, 0, r.byteLength, void 0, t.canOwn) : A("Unsupported data type"), i.close(n);
  }, cwd: () => i.currentPath, chdir(e) {
    var r = i.lookupPath(e, { follow: !0 });
    if (r.node === null)
      throw new i.ErrnoError(44);
    if (!i.isDir(r.node.mode))
      throw new i.ErrnoError(54);
    var t = i.nodePermissions(r.node, "x");
    if (t)
      throw new i.ErrnoError(t);
    i.currentPath = r.path;
  }, createDefaultDirectories() {
    i.mkdir("/tmp"), i.mkdir("/home"), i.mkdir("/home/web_user");
  }, createDefaultDevices() {
    i.mkdir("/dev"), i.registerDevice(i.makedev(1, 3), { read: () => 0, write: (n, o, a, s, l) => s, llseek: () => 0 }), i.mkdev("/dev/null", i.makedev(1, 3)), Q.register(i.makedev(5, 0), Q.default_tty_ops), Q.register(i.makedev(6, 0), Q.default_tty1_ops), i.mkdev("/dev/tty", i.makedev(5, 0)), i.mkdev("/dev/tty1", i.makedev(6, 0));
    var e = new Uint8Array(1024), r = 0, t = () => (r === 0 && (ir(e), r = e.byteLength), e[--r]);
    i.createDevice("/dev", "random", t), i.createDevice("/dev", "urandom", t), i.mkdir("/dev/shm"), i.mkdir("/dev/shm/tmp");
  }, createSpecialDirectories() {
    i.mkdir("/proc");
    var e = i.mkdir("/proc/self");
    i.mkdir("/proc/self/fd"), i.mount({ mount() {
      var r = i.createNode(e, "fd", 16895, 73);
      return r.stream_ops = { llseek: E.stream_ops.llseek }, r.node_ops = { lookup(t, n) {
        var o = +n, a = i.getStreamChecked(o), s = { parent: null, mount: { mountpoint: "fake" }, node_ops: { readlink: () => a.path }, id: o + 1 };
        return s.parent = s, s;
      }, readdir() {
        return Array.from(i.streams.entries()).filter(([t, n]) => n).map(([t, n]) => t.toString());
      } }, r;
    } }, {}, "/proc/self/fd");
  }, createStandardStreams(e, r, t) {
    e ? i.createDevice("/dev", "stdin", e) : i.symlink("/dev/tty", "/dev/stdin"), r ? i.createDevice("/dev", "stdout", null, r) : i.symlink("/dev/tty", "/dev/stdout"), t ? i.createDevice("/dev", "stderr", null, t) : i.symlink("/dev/tty1", "/dev/stderr");
    var n = i.open("/dev/stdin", 0), o = i.open("/dev/stdout", 1), a = i.open("/dev/stderr", 1);
    c(n.fd === 0, `invalid handle for stdin (${n.fd})`), c(o.fd === 1, `invalid handle for stdout (${o.fd})`), c(a.fd === 2, `invalid handle for stderr (${a.fd})`);
  }, staticInit() {
    i.nameTable = new Array(4096), i.mount(E, {}, "/"), i.createDefaultDirectories(), i.createDefaultDevices(), i.createSpecialDirectories(), i.filesystems = { MEMFS: E };
  }, init(e, r, t) {
    c(!i.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), i.initialized = !0, e ??= f.stdin, r ??= f.stdout, t ??= f.stderr, i.createStandardStreams(e, r, t);
  }, quit() {
    i.initialized = !1, _r(0);
    for (var e of i.streams)
      e && i.close(e);
  }, findObject(e, r) {
    var t = i.analyzePath(e, r);
    return t.exists ? t.object : null;
  }, analyzePath(e, r) {
    try {
      var t = i.lookupPath(e, { follow: !r });
      e = t.path;
    } catch {
    }
    var n = { isRoot: !1, exists: !1, error: 0, name: null, path: null, object: null, parentExists: !1, parentPath: null, parentObject: null };
    try {
      var t = i.lookupPath(e, { parent: !0 });
      n.parentExists = !0, n.parentPath = t.path, n.parentObject = t.node, n.name = T.basename(e), t = i.lookupPath(e, { follow: !r }), n.exists = !0, n.path = t.path, n.object = t.node, n.name = t.node.name, n.isRoot = t.path === "/";
    } catch (o) {
      n.error = o.errno;
    }
    return n;
  }, createPath(e, r, t, n) {
    e = typeof e == "string" ? e : i.getPath(e);
    for (var o = r.split("/").reverse(); o.length; ) {
      var a = o.pop();
      if (a) {
        var s = T.join2(e, a);
        try {
          i.mkdir(s);
        } catch (l) {
          if (l.errno != 20) throw l;
        }
        e = s;
      }
    }
    return s;
  }, createFile(e, r, t, n, o) {
    var a = T.join2(typeof e == "string" ? e : i.getPath(e), r), s = sr(n, o);
    return i.create(a, s);
  }, createDataFile(e, r, t, n, o, a) {
    var s = r;
    e && (e = typeof e == "string" ? e : i.getPath(e), s = r ? T.join2(e, r) : e);
    var l = sr(n, o), d = i.create(s, l);
    if (t) {
      if (typeof t == "string") {
        for (var _ = new Array(t.length), v = 0, g = t.length; v < g; ++v) _[v] = t.charCodeAt(v);
        t = _;
      }
      i.chmod(d, l | 146);
      var m = i.open(d, 577);
      i.write(m, t, 0, t.length, 0, a), i.close(m), i.chmod(d, l);
    }
  }, createDevice(e, r, t, n) {
    var o = T.join2(typeof e == "string" ? e : i.getPath(e), r), a = sr(!!t, !!n);
    i.createDevice.major ??= 64;
    var s = i.makedev(i.createDevice.major++, 0);
    return i.registerDevice(s, { open(l) {
      l.seekable = !1;
    }, close(l) {
      n?.buffer?.length && n(10);
    }, read(l, d, _, v, g) {
      for (var m = 0, p = 0; p < v; p++) {
        var k;
        try {
          k = t();
        } catch {
          throw new i.ErrnoError(29);
        }
        if (k === void 0 && m === 0)
          throw new i.ErrnoError(6);
        if (k == null) break;
        m++, d[_ + p] = k;
      }
      return m && (l.node.atime = Date.now()), m;
    }, write(l, d, _, v, g) {
      for (var m = 0; m < v; m++)
        try {
          n(d[_ + m]);
        } catch {
          throw new i.ErrnoError(29);
        }
      return v && (l.node.mtime = l.node.ctime = Date.now()), m;
    } }), i.mkdev(o, a, s);
  }, forceLoadFile(e) {
    if (e.isDevice || e.isFolder || e.link || e.contents) return !0;
    if (globalThis.XMLHttpRequest)
      A("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    else
      try {
        e.contents = Pe(e.url);
      } catch {
        throw new i.ErrnoError(29);
      }
  }, createLazyFile(e, r, t, n, o) {
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
        m.open("HEAD", t, !1), m.send(null), m.status >= 200 && m.status < 300 || m.status === 304 || A("Couldn't load " + t + ". Status: " + m.status);
        var p = Number(m.getResponseHeader("Content-length")), k, S = (k = m.getResponseHeader("Accept-Ranges")) && k === "bytes", I = (k = m.getResponseHeader("Content-Encoding")) && k === "gzip", W = 1024 * 1024;
        S || (W = p);
        var x = (H, ye) => {
          H > ye && A("invalid range (" + H + ", " + ye + ") or no bytes requested!"), ye > p - 1 && A("only " + p + " bytes available! programmer error!");
          var O = new XMLHttpRequest();
          return O.open("GET", t, !1), p !== W && O.setRequestHeader("Range", "bytes=" + H + "-" + ye), O.responseType = "arraybuffer", O.overrideMimeType && O.overrideMimeType("text/plain; charset=x-user-defined"), O.send(null), O.status >= 200 && O.status < 300 || O.status === 304 || A("Couldn't load " + t + ". Status: " + O.status), O.response !== void 0 ? new Uint8Array(O.response || []) : ar(O.responseText || "");
        }, Fe = this;
        Fe.setDataGetter((H) => {
          var ye = H * W, O = (H + 1) * W - 1;
          return O = Math.min(O, p - 1), typeof Fe.chunks[H] > "u" && (Fe.chunks[H] = x(ye, O)), typeof Fe.chunks[H] > "u" && A("doXHR failed!"), Fe.chunks[H];
        }), (I || !p) && (W = p = 1, p = this.getter(0).length, W = p, J("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = p, this._chunkSize = W, this.lengthKnown = !0;
      }
      get length() {
        return this.lengthKnown || this.cacheLength(), this._length;
      }
      get chunkSize() {
        return this.lengthKnown || this.cacheLength(), this._chunkSize;
      }
    }
    if (globalThis.XMLHttpRequest) {
      Y || A("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
      var s = new a(), l = { isDevice: !1, contents: s };
    } else
      var l = { isDevice: !1, url: t };
    var d = i.createFile(e, r, l, n, o);
    l.contents ? d.contents = l.contents : l.url && (d.contents = null, d.url = l.url), Object.defineProperties(d, { usedBytes: { get: function() {
      return this.contents.length;
    } } });
    var _ = {};
    for (const [g, m] of Object.entries(d.stream_ops))
      _[g] = (...p) => (i.forceLoadFile(d), m(...p));
    function v(g, m, p, k, S) {
      var I = g.node.contents;
      if (S >= I.length) return 0;
      var W = Math.min(I.length - S, k);
      if (c(W >= 0), I.slice)
        for (var x = 0; x < W; x++)
          m[p + x] = I[S + x];
      else
        for (var x = 0; x < W; x++)
          m[p + x] = I.get(S + x);
      return W;
    }
    return _.read = (g, m, p, k, S) => (i.forceLoadFile(d), v(g, m, p, k, S)), _.mmap = (g, m, p, k, S) => {
      i.forceLoadFile(d);
      var I = Jr(m);
      if (!I)
        throw new i.ErrnoError(48);
      return v(g, (u(), N), I, m, p), { ptr: I, allocated: !0 };
    }, d.stream_ops = _, d;
  }, absolutePath() {
    A("FS.absolutePath has been removed; use PATH_FS.resolve instead");
  }, createFolder() {
    A("FS.createFolder has been removed; use FS.mkdir instead");
  }, createLink() {
    A("FS.createLink has been removed; use FS.symlink instead");
  }, joinPath() {
    A("FS.joinPath has been removed; use PATH.join instead");
  }, mmapAlloc() {
    A("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
  }, standardizePath() {
    A("FS.standardizePath has been removed; use PATH.normalize instead");
  } }, P = { DEFAULT_POLLMASK: 5, calculateAt(e, r, t) {
    if (T.isAbs(r))
      return r;
    var n;
    if (e === -100)
      n = i.cwd();
    else {
      var o = P.getStreamFromFD(e);
      n = o.path;
    }
    if (r.length == 0) {
      if (!t)
        throw new i.ErrnoError(44);
      return n;
    }
    return n + "/" + r;
  }, writeStat(e, r) {
    (u(), h)[e >> 2] = r.dev, (u(), h)[e + 4 >> 2] = r.mode, (u(), h)[e + 8 >> 2] = r.nlink, (u(), h)[e + 12 >> 2] = r.uid, (u(), h)[e + 16 >> 2] = r.gid, (u(), h)[e + 20 >> 2] = r.rdev, (u(), M)[e + 24 >> 3] = BigInt(r.size), (u(), C)[e + 32 >> 2] = 4096, (u(), C)[e + 36 >> 2] = r.blocks;
    var t = r.atime.getTime(), n = r.mtime.getTime(), o = r.ctime.getTime();
    return (u(), M)[e + 40 >> 3] = BigInt(Math.floor(t / 1e3)), (u(), h)[e + 48 >> 2] = t % 1e3 * 1e3 * 1e3, (u(), M)[e + 56 >> 3] = BigInt(Math.floor(n / 1e3)), (u(), h)[e + 64 >> 2] = n % 1e3 * 1e3 * 1e3, (u(), M)[e + 72 >> 3] = BigInt(Math.floor(o / 1e3)), (u(), h)[e + 80 >> 2] = o % 1e3 * 1e3 * 1e3, (u(), M)[e + 88 >> 3] = BigInt(r.ino), 0;
  }, writeStatFs(e, r) {
    (u(), h)[e + 4 >> 2] = r.bsize, (u(), h)[e + 60 >> 2] = r.bsize, (u(), M)[e + 8 >> 3] = BigInt(r.blocks), (u(), M)[e + 16 >> 3] = BigInt(r.bfree), (u(), M)[e + 24 >> 3] = BigInt(r.bavail), (u(), M)[e + 32 >> 3] = BigInt(r.files), (u(), M)[e + 40 >> 3] = BigInt(r.ffree), (u(), h)[e + 48 >> 2] = r.fsid, (u(), h)[e + 64 >> 2] = r.flags, (u(), h)[e + 56 >> 2] = r.namelen;
  }, doMsync(e, r, t, n, o) {
    if (!i.isFile(r.node.mode))
      throw new i.ErrnoError(43);
    if (n & 2)
      return 0;
    var a = (u(), L).slice(e, e + t);
    i.msync(r, a, o, t, n);
  }, getStreamFromFD(e) {
    var r = i.getStreamChecked(e);
    return r;
  }, varargs: void 0, getStr(e) {
    var r = V(e);
    return r;
  } };
  function rt(e, r, t) {
    if (y) return D(3, 0, 1, e, r, t);
    P.varargs = t;
    try {
      var n = P.getStreamFromFD(e);
      switch (r) {
        case 0: {
          var o = Re();
          if (o < 0)
            return -28;
          for (; i.streams[o]; )
            o++;
          var a;
          return a = i.dupStream(n, o), a.fd;
        }
        case 1:
        case 2:
          return 0;
        case 3:
          return n.flags;
        case 4: {
          var o = Re();
          return n.flags |= o, 0;
        }
        case 12: {
          var o = _e(), s = 0;
          return (u(), re)[o + s >> 1] = 2, 0;
        }
        case 13:
        case 14:
          return 0;
      }
      return -28;
    } catch (l) {
      if (typeof i > "u" || l.name !== "ErrnoError") throw l;
      return -l.errno;
    }
  }
  function tt(e, r) {
    if (y) return D(4, 0, 1, e, r);
    try {
      return P.writeStat(r, i.fstat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function nt(e, r, t) {
    if (y) return D(5, 0, 1, e, r, t);
    P.varargs = t;
    try {
      var n = P.getStreamFromFD(e);
      switch (r) {
        case 21509:
          return n.tty ? 0 : -59;
        case 21505: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tcgets) {
            var o = n.tty.ops.ioctl_tcgets(n), a = _e();
            (u(), C)[a >> 2] = o.c_iflag || 0, (u(), C)[a + 4 >> 2] = o.c_oflag || 0, (u(), C)[a + 8 >> 2] = o.c_cflag || 0, (u(), C)[a + 12 >> 2] = o.c_lflag || 0;
            for (var s = 0; s < 32; s++)
              (u(), N)[a + s + 17] = o.c_cc[s] || 0;
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
            for (var a = _e(), l = (u(), C)[a >> 2], d = (u(), C)[a + 4 >> 2], _ = (u(), C)[a + 8 >> 2], v = (u(), C)[a + 12 >> 2], g = [], s = 0; s < 32; s++)
              g.push((u(), N)[a + s + 17]);
            return n.tty.ops.ioctl_tcsets(n.tty, r, { c_iflag: l, c_oflag: d, c_cflag: _, c_lflag: v, c_cc: g });
          }
          return 0;
        }
        case 21519: {
          if (!n.tty) return -59;
          var a = _e();
          return (u(), C)[a >> 2] = 0, 0;
        }
        case 21520:
          return n.tty ? -28 : -59;
        case 21537:
        case 21531: {
          var a = _e();
          return i.ioctl(n, r, a);
        }
        case 21523: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tiocgwinsz) {
            var m = n.tty.ops.ioctl_tiocgwinsz(n.tty), a = _e();
            (u(), re)[a >> 1] = m[0], (u(), re)[a + 2 >> 1] = m[1];
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
      if (typeof i > "u" || p.name !== "ErrnoError") throw p;
      return -p.errno;
    }
  }
  function it(e, r) {
    if (y) return D(6, 0, 1, e, r);
    try {
      return e = P.getStr(e), P.writeStat(r, i.lstat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function ot(e, r, t, n) {
    if (y) return D(7, 0, 1, e, r, t, n);
    try {
      r = P.getStr(r);
      var o = n & 256, a = n & 4096;
      return n = n & -6401, c(!n, `unknown flags in __syscall_newfstatat: ${n}`), r = P.calculateAt(e, r, a), P.writeStat(t, o ? i.lstat(r) : i.stat(r));
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return -s.errno;
    }
  }
  function at(e, r, t, n) {
    if (y) return D(8, 0, 1, e, r, t, n);
    P.varargs = n;
    try {
      r = P.getStr(r), r = P.calculateAt(e, r);
      var o = n ? Re() : 0;
      return i.open(r, t, o).fd;
    } catch (a) {
      if (typeof i > "u" || a.name !== "ErrnoError") throw a;
      return -a.errno;
    }
  }
  function st(e, r) {
    if (y) return D(9, 0, 1, e, r);
    try {
      return e = P.getStr(e), P.writeStat(r, i.stat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  var An = () => A("native code called abort()"), U = (e) => {
    for (var r = ""; ; ) {
      var t = (u(), L)[e++];
      if (!t) return r;
      r += String.fromCharCode(t);
    }
  }, ve = {}, he = {}, De = {}, Pn = class extends Error {
    constructor(r) {
      super(r), this.name = "BindingError";
    }
  }, $ = (e) => {
    throw new Pn(e);
  };
  function Cn(e, r, t = {}) {
    var n = r.name;
    if (e || $(`type "${n}" must have a positive integer typeid pointer`), he.hasOwnProperty(e)) {
      if (t.ignoreDuplicateRegistrations)
        return;
      $(`Cannot register type '${n}' twice`);
    }
    if (he[e] = r, delete De[e], ve.hasOwnProperty(e)) {
      var o = ve[e];
      delete ve[e], o.forEach((a) => a());
    }
  }
  function z(e, r, t = {}) {
    return Cn(e, r, t);
  }
  var lt = (e, r, t) => {
    switch (r) {
      case 1:
        return t ? (n) => (u(), N)[n] : (n) => (u(), L)[n];
      case 2:
        return t ? (n) => (u(), re)[n >> 1] : (n) => (u(), be)[n >> 1];
      case 4:
        return t ? (n) => (u(), C)[n >> 2] : (n) => (u(), h)[n >> 2];
      case 8:
        return t ? (n) => (u(), M)[n >> 3] : (n) => (u(), Ar)[n >> 3];
      default:
        throw new TypeError(`invalid integer width (${r}): ${e}`);
    }
  }, Ne = (e) => {
    if (e === null)
      return "null";
    var r = typeof e;
    return r === "object" || r === "array" || r === "function" ? e.toString() : "" + e;
  }, dt = (e, r, t, n) => {
    if (r < t || r > n)
      throw new TypeError(`Passing a number "${Ne(r)}" from JS side to C/C++ side to an argument of type "${e}", which is outside the valid range [${t}, ${n}]!`);
  }, In = (e, r, t, n, o) => {
    r = U(r);
    const a = n === 0n;
    let s = (l) => l;
    if (a) {
      const l = t * 8;
      s = (d) => BigInt.asUintN(l, d), o = s(o);
    }
    z(e, { name: r, fromWireType: s, toWireType: (l, d) => {
      if (typeof d == "number")
        d = BigInt(d);
      else if (typeof d != "bigint")
        throw new TypeError(`Cannot convert "${Ne(d)}" to ${this.name}`);
      return dt(r, d, n, o), d;
    }, readValueFromPointer: lt(r, t, !a), destructorFunction: null });
  }, Mn = (e, r, t, n) => {
    r = U(r), z(e, { name: r, fromWireType: function(o) {
      return !!o;
    }, toWireType: function(o, a) {
      return a ? t : n;
    }, readValueFromPointer: function(o) {
      return this.fromWireType((u(), L)[o]);
    }, destructorFunction: null });
  }, ct = [], G = [0, 1, , 1, null, 1, !0, 1, !1, 1], Rn = (e) => {
    e > 9 && --G[e + 1] === 0 && (c(G[e] !== void 0, "Decref for unallocated handle."), G[e] = void 0, ct.push(e));
  }, ut = { toValue: (e) => (e || $(`Cannot use deleted val. handle = ${e}`), c(e === 2 || G[e] !== void 0 && e % 2 === 0, `invalid handle: ${e}`), G[e]), toHandle: (e) => {
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
        const r = ct.pop() || G.length;
        return G[r] = e, G[r + 1] = 1, r;
      }
    }
  } };
  function lr(e) {
    return this.fromWireType((u(), h)[e >> 2]);
  }
  var Dn = { name: "emscripten::val", fromWireType: (e) => {
    var r = ut.toValue(e);
    return Rn(e), r;
  }, toWireType: (e, r) => ut.toHandle(r), readValueFromPointer: lr, destructorFunction: null }, Nn = (e) => z(e, Dn), On = (e, r) => {
    switch (r) {
      case 4:
        return function(t) {
          return this.fromWireType((u(), Fr)[t >> 2]);
        };
      case 8:
        return function(t) {
          return this.fromWireType((u(), Se)[t >> 3]);
        };
      default:
        throw new TypeError(`invalid float width (${r}): ${e}`);
    }
  }, Wn = (e, r, t) => {
    r = U(r), z(e, { name: r, fromWireType: (n) => n, toWireType: (n, o) => {
      if (typeof o != "number" && typeof o != "boolean")
        throw new TypeError(`Cannot convert ${Ne(o)} to ${this.name}`);
      return o;
    }, readValueFromPointer: On(r, t), destructorFunction: null });
  }, ft = (e, r) => Object.defineProperty(r, "name", { value: e }), xn = (e) => {
    for (; e.length; ) {
      var r = e.pop(), t = e.pop();
      t(r);
    }
  };
  function _t(e) {
    for (var r = 1; r < e.length; ++r)
      if (e[r] !== null && e[r].destructorFunction === void 0)
        return !0;
    return !1;
  }
  function Ln(e, r, t, n, o) {
    if (e < r || e > t) {
      var a = r == t ? r : `${r} to ${t}`;
      o(`function ${n} called with ${e} arguments, expected ${a}`);
    }
  }
  function Un(e, r, t, n) {
    for (var o = _t(e), a = e.length - 2, s = [], l = ["fn"], d = 0; d < a; ++d)
      s.push(`arg${d}`), l.push(`arg${d}Wired`);
    s = s.join(","), l = l.join(",");
    var _ = `return function (${s}) {
`;
    _ += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, o && (_ += `var destructors = [];
`);
    for (var v = o ? "destructors" : "null", g = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "fromRetWire", "toClassParamWire"], d = 0; d < a; ++d) {
      var m = `toArg${d}Wire`;
      _ += `var arg${d}Wired = ${m}(${v}, arg${d});
`, g.push(m);
    }
    _ += (t || n ? "var rv = " : "") + `invoker(${l});
`;
    var p = t ? "rv" : "";
    if (_ += `function onDone(${p}) {
`, o)
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
  function $n(e) {
    for (var r = e.length - 2, t = e.length - 1; t >= 2 && e[t].optional; --t)
      r--;
    return r;
  }
  function Bn(e, r, t, n, o, a) {
    var s = r.length;
    s < 2 && $("argTypes array size mismatch! Must at least get return value and 'this' types!");
    for (var l = r[1] !== null && t !== null, d = _t(r), _ = !r[0].isVoid, v = s - 2, g = $n(r), m = r[0], p = r[1], k = [e, $, n, o, xn, m.fromWireType.bind(m), p?.toWireType.bind(p)], S = 2; S < s; ++S) {
      var I = r[S];
      k.push(I.toWireType.bind(I));
    }
    if (!d)
      for (var S = 2; S < r.length; ++S)
        r[S].destructorFunction !== null && k.push(r[S].destructorFunction);
    k.push(Ln, g, v);
    var x = Un(r, l, _, a)(...k);
    return ft(e, x);
  }
  var zn = (e, r, t) => {
    if (e[r].overloadTable === void 0) {
      var n = e[r];
      e[r] = function(...o) {
        return e[r].overloadTable.hasOwnProperty(o.length) || $(`Function '${t}' called with an invalid number of arguments (${o.length}) - expects one of (${e[r].overloadTable})!`), e[r].overloadTable[o.length].apply(this, o);
      }, e[r].overloadTable = [], e[r].overloadTable[n.argCount] = n;
    }
  }, jn = (e, r, t) => {
    f.hasOwnProperty(e) ? ((t === void 0 || f[e].overloadTable !== void 0 && f[e].overloadTable[t] !== void 0) && $(`Cannot register public name '${e}' twice`), zn(f, e, e), f[e].overloadTable.hasOwnProperty(t) && $(`Cannot register multiple overloads of a function with the same number of arguments (${t})!`), f[e].overloadTable[t] = r) : (f[e] = r, f[e].argCount = t);
  }, Hn = (e, r) => {
    for (var t = [], n = 0; n < e; n++)
      t.push((u(), h)[r + n * 4 >> 2]);
    return t;
  }, Vn = class extends Error {
    constructor(r) {
      super(r), this.name = "InternalError";
    }
  }, mt = (e) => {
    throw new Vn(e);
  }, Gn = (e, r, t) => {
    f.hasOwnProperty(e) || mt("Replacing nonexistent public symbol"), f[e].overloadTable !== void 0 && t !== void 0 ? f[e].overloadTable[t] = r : (f[e] = r, f[e].argCount = t);
  }, qn = (e, r, t = !1) => {
    e = U(e);
    function n() {
      var a = jr(r);
      return t && (a = WebAssembly.promising(a)), a;
    }
    var o = n();
    return typeof o != "function" && $(`unknown function pointer with signature ${e}: ${r}`), o;
  };
  class Kn extends Error {
  }
  var Xn = (e) => {
    var r = Nt(e), t = U(r);
    return K(r), t;
  }, Yn = (e, r) => {
    var t = [], n = {};
    function o(a) {
      if (!n[a] && !he[a]) {
        if (De[a]) {
          De[a].forEach(o);
          return;
        }
        t.push(a), n[a] = !0;
      }
    }
    throw r.forEach(o), new Kn(`${e}: ` + t.map(Xn).join([", "]));
  }, Jn = (e, r, t) => {
    e.forEach((l) => De[l] = r);
    function n(l) {
      var d = t(l);
      d.length !== e.length && mt("Mismatched type converter count");
      for (var _ = 0; _ < e.length; ++_)
        z(e[_], d[_]);
    }
    var o = new Array(r.length), a = [], s = 0;
    for (let [l, d] of r.entries())
      he.hasOwnProperty(d) ? o[l] = he[d] : (a.push(d), ve.hasOwnProperty(d) || (ve[d] = []), ve[d].push(() => {
        o[l] = he[d], ++s, s === a.length && n(o);
      }));
    a.length === 0 && n(o);
  }, Zn = (e) => {
    e = e.trim();
    const r = e.indexOf("(");
    return r === -1 ? e : (c(e.endsWith(")"), "Parentheses for argument names should match."), e.slice(0, r));
  }, Qn = (e, r, t, n, o, a, s, l) => {
    var d = Hn(r, t);
    e = U(e), e = Zn(e), o = qn(n, o, s), jn(e, function() {
      Yn(`Cannot call ${e} due to unbound types`, d);
    }, r - 1), Jn([], d, (_) => {
      var v = [_[0], null].concat(_.slice(1));
      return Gn(e, Bn(e, v, null, o, a, s), r - 1), [];
    });
  }, ei = (e, r, t, n, o) => {
    r = U(r);
    const a = n === 0;
    let s = (d) => d;
    if (a) {
      var l = 32 - 8 * t;
      s = (d) => d << l >>> l, o = s(o);
    }
    z(e, { name: r, fromWireType: s, toWireType: (d, _) => {
      if (typeof _ != "number" && typeof _ != "boolean")
        throw new TypeError(`Cannot convert "${Ne(_)}" to ${r}`);
      return dt(r, _, n, o), _;
    }, readValueFromPointer: lt(r, t, n !== 0), destructorFunction: null });
  }, ri = (e, r, t) => {
    var n = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array], o = n[r];
    function a(s) {
      var l = (u(), h)[s >> 2], d = (u(), h)[s + 4 >> 2];
      return new o((u(), N).buffer, d, l);
    }
    t = U(t), z(e, { name: t, fromWireType: a, readValueFromPointer: a }, { ignoreDuplicateRegistrations: !0 });
  }, q = (e, r, t) => (c(typeof t == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), Xr(e, (u(), L), r, t)), ti = (e, r) => {
    r = U(r), z(e, { name: r, fromWireType(t) {
      var n = (u(), h)[t >> 2], o = t + 4, a;
      return a = V(o, n, !0), K(t), a;
    }, toWireType(t, n) {
      n instanceof ArrayBuffer && (n = new Uint8Array(n));
      var o, a = typeof n == "string";
      a || ArrayBuffer.isView(n) && n.BYTES_PER_ELEMENT == 1 || $("Cannot pass non-string to std::string"), a ? o = oe(n) : o = n.length;
      var s = $e(4 + o + 1), l = s + 4;
      return (u(), h)[s >> 2] = o, a ? q(n, l, o + 1) : (u(), L).set(n, l), t !== null && t.push(K, s), s;
    }, readValueFromPointer: lr, destructorFunction(t) {
      K(t);
    } });
  }, vt = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, ni = (e, r, t) => {
    c(e % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
    var n = e >> 1, o = Gr((u(), be), n, r / 2, t);
    if (o - n > 16 && vt) return vt.decode((u(), be).slice(n, o));
    for (var a = "", s = n; s < o; ++s) {
      var l = (u(), be)[s];
      a += String.fromCharCode(l);
    }
    return a;
  }, ii = (e, r, t) => {
    if (c(r % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), c(typeof t == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 2) return 0;
    t -= 2;
    for (var n = r, o = t < e.length * 2 ? t / 2 : e.length, a = 0; a < o; ++a) {
      var s = e.charCodeAt(a);
      (u(), re)[r >> 1] = s, r += 2;
    }
    return (u(), re)[r >> 1] = 0, r - n;
  }, oi = (e) => e.length * 2, ai = (e, r, t) => {
    c(e % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
    for (var n = "", o = e >> 2, a = 0; !(a >= r / 4); a++) {
      var s = (u(), h)[o + a];
      if (!s && !t) break;
      n += String.fromCodePoint(s);
    }
    return n;
  }, si = (e, r, t) => {
    if (c(r % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), c(typeof t == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 4) return 0;
    for (var n = r, o = n + t - 4, a = 0; a < e.length; ++a) {
      var s = e.codePointAt(a);
      if (s > 65535 && a++, (u(), C)[r >> 2] = s, r += 4, r + 4 > o) break;
    }
    return (u(), C)[r >> 2] = 0, r - n;
  }, li = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.codePointAt(t);
      n > 65535 && t++, r += 4;
    }
    return r;
  }, di = (e, r, t) => {
    t = U(t);
    var n, o, a;
    r === 2 ? (n = ni, o = ii, a = oi) : (c(r === 4, "only 2-byte and 4-byte strings are currently supported"), n = ai, o = si, a = li), z(e, { name: t, fromWireType: (s) => {
      var l = (u(), h)[s >> 2], d = n(s + 4, l * r, !0);
      return K(s), d;
    }, toWireType: (s, l) => {
      typeof l != "string" && $(`Cannot pass non-string to C++ string type ${t}`);
      var d = a(l), _ = $e(4 + d + r);
      return (u(), h)[_ >> 2] = d / r, o(l, _ + 4, d + r), s !== null && s.push(K, _), _;
    }, readValueFromPointer: lr, destructorFunction(s) {
      K(s);
    } });
  }, ci = (e, r) => {
    r = U(r), z(e, { isVoid: !0, name: r, fromWireType: () => {
    }, toWireType: (t, n) => {
    } });
  }, ui = (e) => {
    mr(e, !Y, 1, !Ae, 65536, !1), w.threadInitTLS();
  }, ht = (e) => {
    if (e instanceof Mr || e == "unwind")
      return le;
    we(), e instanceof WebAssembly.RuntimeError && pr() <= 0 && b("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), Er(1, e);
  }, fi = () => {
    if (!Me())
      try {
        if (y) {
          pe() && hr(le);
          return;
        }
        rr(le);
      } catch (e) {
        ht(e);
      }
  }, pt = (e) => {
    if (se) {
      b("user callback triggered after runtime exited or application aborted.  Ignoring.");
      return;
    }
    try {
      e(), fi();
    } catch (r) {
      ht(r);
    }
  }, dr = (e) => {
    if (Atomics.waitAsync) {
      var r = Atomics.waitAsync((u(), C), e >> 2, e);
      c(r.async), r.value.then(Oe);
      var t = e + 128;
      Atomics.store((u(), C), t >> 2, 1);
    }
  }, Oe = () => pt(() => {
    var e = pe();
    e && (dr(e), Bt());
  }), _i = (e, r) => {
    if (e == r)
      setTimeout(Oe);
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
  }, We = [], mi = (e, r, t, n, o) => {
    n /= 2, We.length = n;
    for (var a = o >> 3, s = 0; s < n; s++)
      (u(), M)[a + 2 * s] ? We[s] = (u(), M)[a + 2 * s + 1] : We[s] = (u(), Se)[a + 2 * s + 1];
    var l = r ? fr[r] : qi[e];
    c(!(e && r)), c(l.length == n, "Call args mismatch in _emscripten_receive_on_main_thread_js"), w.currentProxiedOperationCallerThread = t;
    var d = l(...We);
    return w.currentProxiedOperationCallerThread = 0, c(typeof d != "bigint"), d;
  }, vi = (e) => {
    y ? postMessage({ cmd: "cleanupThread", thread: e }) : Dr(e);
  }, hi = (e) => {
  }, pi = 9007199254740992, gi = -9007199254740992, cr = (e) => e < gi || e > pi ? NaN : Number(e);
  function gt(e, r, t, n, o, a, s) {
    if (y) return D(10, 0, 1, e, r, t, n, o, a, s);
    o = cr(o);
    try {
      c(!isNaN(o));
      var l = P.getStreamFromFD(n), d = i.mmap(l, e, o, r, t), _ = d.ptr;
      return (u(), C)[a >> 2] = d.allocated, (u(), h)[s >> 2] = _, 0;
    } catch (v) {
      if (typeof i > "u" || v.name !== "ErrnoError") throw v;
      return -v.errno;
    }
  }
  function yt(e, r, t, n, o, a) {
    if (y) return D(11, 0, 1, e, r, t, n, o, a);
    a = cr(a);
    try {
      var s = P.getStreamFromFD(o);
      t & 2 && P.doMsync(e, s, r, n, a);
    } catch (l) {
      if (typeof i > "u" || l.name !== "ErrnoError") throw l;
      return -l.errno;
    }
  }
  var yi = (e, r, t, n) => {
    var o = (/* @__PURE__ */ new Date()).getFullYear(), a = new Date(o, 0, 1), s = new Date(o, 6, 1), l = a.getTimezoneOffset(), d = s.getTimezoneOffset(), _ = Math.max(l, d);
    (u(), h)[e >> 2] = _ * 60, (u(), C)[r >> 2] = +(l != d);
    var v = (p) => {
      var k = p >= 0 ? "-" : "+", S = Math.abs(p), I = String(Math.floor(S / 60)).padStart(2, "0"), W = String(S % 60).padStart(2, "0");
      return `UTC${k}${I}${W}`;
    }, g = v(l), m = v(d);
    c(g), c(m), c(oe(g) <= 16, `timezone name truncated to fit in TZNAME_MAX (${g})`), c(oe(m) <= 16, `timezone name truncated to fit in TZNAME_MAX (${m})`), d < l ? (q(g, t, 17), q(m, n, 17)) : (q(g, n, 17), q(m, t, 17));
  }, Et = () => performance.timeOrigin + performance.now(), Ei = () => Date.now(), wi = (e) => e >= 0 && e <= 3;
  function bi(e, r, t) {
    if (!wi(e))
      return 28;
    var n;
    e === 0 ? n = Ei() : n = Et();
    var o = Math.round(n * 1e3 * 1e3);
    return (u(), M)[t >> 3] = BigInt(o), 0;
  }
  var xe = [], Si = (e, r) => {
    c(Array.isArray(xe)), c(r % 16 == 0), xe.length = 0;
    for (var t; t = (u(), L)[e++]; ) {
      var n = String.fromCharCode(t), o = ["d", "f", "i", "p"];
      o.push("j"), c(o.includes(n), `Invalid character ${t}("${n}") in readEmAsmArgs! Use only [${o}], and do not specify "v" for void return argument.`);
      var a = t != 105;
      a &= t != 112, r += a && r % 8 ? 4 : 0, xe.push(t == 112 ? (u(), h)[r >> 2] : t == 106 ? (u(), M)[r >> 3] : t == 105 ? (u(), C)[r >> 2] : (u(), Se)[r >> 3]), r += a ? 8 : 4;
    }
    return xe;
  }, ki = (e, r, t) => {
    var n = Si(r, t);
    return c(fr.hasOwnProperty(e), `No EM_ASM constant found at address ${e}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), fr[e](...n);
  }, Ti = (e, r, t) => ki(e, r, t), Fi = () => {
    Y || Z("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
  }, Ai = (e, r) => b(V(e, r)), wt = () => {
    ue += 1;
  }, Pi = () => {
    throw wt(), "unwind";
  }, bt = () => 2147483648, Ci = () => bt(), Ii = () => navigator.hardwareConcurrency, ae = {}, St = (e) => {
    var r = oe(e) + 1, t = $e(r);
    return t && q(e, t, r), t;
  }, Le = (e) => {
    var r;
    if (r = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(e))
      return +r[1];
    if (r = /\bwasm-function\[(\d+)\]:(\d+)/.exec(e))
      Z("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
    else if (r = /:(\d+):\d+(?:\)|$)/.exec(e))
      return 2147483648 | +r[1];
    return 0;
  }, kt = (e) => {
    for (var r of e) {
      var t = Le(r);
      t && (ae[t] = r);
    }
  }, Tt = () => new Error().stack.toString(), Mi = () => {
    var e = Tt().split(`
`);
    return e[0] == "Error" && e.shift(), kt(e), ae.last_addr = Le(e[3]), ae.last_stack = e, ae.last_addr;
  }, Ue = (e) => {
    var r = ae[e];
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
    return K(Ue.ret ?? 0), Ue.ret = St(t), Ue.ret;
  }, Ri = (e) => {
    var r = B.buffer.byteLength, t = (e - r + 65535) / 65536 | 0;
    try {
      return B.grow(t), Ie(), 1;
    } catch (n) {
      b(`growMemory: Attempted to grow heap from ${r} bytes to ${e} bytes, but got error: ${n}`);
    }
  }, Di = (e) => {
    var r = (u(), L).length;
    if (e >>>= 0, e <= r)
      return !1;
    var t = bt();
    if (e > t)
      return b(`Cannot enlarge memory, requested ${e} bytes, but the limit is ${t} bytes!`), !1;
    for (var n = 1; n <= 4; n *= 2) {
      var o = r * (1 + 0.2 / n);
      o = Math.min(o, e + 100663296);
      var a = Math.min(t, Yr(Math.max(e, o), 65536)), s = Ri(a);
      if (s)
        return !0;
    }
    return b(`Failed to grow the heap from ${r} bytes to ${a} bytes, not enough memory!`), !1;
  }, Ni = (e, r, t) => {
    var n;
    ae.last_addr == e ? n = ae.last_stack : (n = Tt().split(`
`), n[0] == "Error" && n.shift(), kt(n));
    for (var o = 3; n[o] && Le(n[o]) != e; )
      ++o;
    for (var a = 0; a < t && n[a + o]; ++a)
      (u(), C)[r + a * 4 >> 2] = Le(n[a + o]);
    return a;
  }, ur = {}, Oi = () => yr || "./this.program", Te = () => {
    if (!Te.strings) {
      var e = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", r = { USER: "web_user", LOGNAME: "web_user", PATH: "/", PWD: "/", HOME: "/home/web_user", LANG: e, _: Oi() };
      for (var t in ur)
        ur[t] === void 0 ? delete r[t] : r[t] = ur[t];
      var n = [];
      for (var t in r)
        n.push(`${t}=${r[t]}`);
      Te.strings = n;
    }
    return Te.strings;
  };
  function Ft(e, r) {
    if (y) return D(12, 0, 1, e, r);
    var t = 0, n = 0;
    for (var o of Te()) {
      var a = r + t;
      (u(), h)[e + n >> 2] = a, t += q(o, a, 1 / 0) + 1, n += 4;
    }
    return 0;
  }
  function At(e, r) {
    if (y) return D(13, 0, 1, e, r);
    var t = Te();
    (u(), h)[e >> 2] = t.length;
    var n = 0;
    for (var o of t)
      n += oe(o) + 1;
    return (u(), h)[r >> 2] = n, 0;
  }
  function Pt(e) {
    if (y) return D(14, 0, 1, e);
    try {
      var r = P.getStreamFromFD(e);
      return i.close(r), 0;
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var Wi = (e, r, t, n) => {
    for (var o = 0, a = 0; a < t; a++) {
      var s = (u(), h)[r >> 2], l = (u(), h)[r + 4 >> 2];
      r += 8;
      var d = i.read(e, (u(), N), s, l, n);
      if (d < 0) return -1;
      if (o += d, d < l) break;
    }
    return o;
  };
  function Ct(e, r, t, n) {
    if (y) return D(15, 0, 1, e, r, t, n);
    try {
      var o = P.getStreamFromFD(e), a = Wi(o, r, t);
      return (u(), h)[n >> 2] = a, 0;
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function It(e, r, t, n) {
    if (y) return D(16, 0, 1, e, r, t, n);
    r = cr(r);
    try {
      if (isNaN(r)) return 61;
      var o = P.getStreamFromFD(e);
      return i.llseek(o, r, t), (u(), M)[n >> 3] = BigInt(o.position), o.getdents && r === 0 && t === 0 && (o.getdents = null), 0;
    } catch (a) {
      if (typeof i > "u" || a.name !== "ErrnoError") throw a;
      return a.errno;
    }
  }
  var xi = (e, r, t, n) => {
    for (var o = 0, a = 0; a < t; a++) {
      var s = (u(), h)[r >> 2], l = (u(), h)[r + 4 >> 2];
      r += 8;
      var d = i.write(e, (u(), N), s, l, n);
      if (d < 0) return -1;
      if (o += d, d < l)
        break;
    }
    return o;
  };
  function Mt(e, r, t, n) {
    if (y) return D(17, 0, 1, e, r, t, n);
    try {
      var o = P.getStreamFromFD(e), a = xi(o, r, t);
      return (u(), h)[n >> 2] = a, 0;
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function Li(e, r) {
    try {
      return ir((u(), L).subarray(e, e + r)), 0;
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var Ui = () => {
    c(ue > 0), ue -= 1;
  }, j = { instrumentWasmImports(e) {
    c("Suspending" in WebAssembly, "JSPI not supported by current environment. Perhaps it needs to be enabled via flags?");
    var r = /^(invoke_.*|__asyncjs__.*)$/;
    for (let [t, n] of Object.entries(e))
      typeof n == "function" && (n.isAsync || r.test(t)) && (e[t] = n = new WebAssembly.Suspending(n));
  }, instrumentFunction(e) {
    var r = (...t) => e(...t);
    return r = ft(`__asyncify_wrapper_${e.name}`, r), r;
  }, instrumentWasmExports(e) {
    var r = /^(solve_model|validate_model|main|__main_argc_argv)$/;
    j.asyncExports = /* @__PURE__ */ new Set();
    var t = {};
    for (let [o, a] of Object.entries(e))
      if (typeof a == "function") {
        r.test(o) && (j.asyncExports.add(a), a = j.makeAsyncFunction(a));
        var n = j.instrumentFunction(a);
        t[o] = n;
      } else
        t[o] = a;
    return t;
  }, asyncExports: null, isAsyncExport(e) {
    return j.asyncExports?.has(e);
  }, handleAsync: async (e) => {
    wt();
    try {
      return await e();
    } finally {
      Ui();
    }
  }, handleSleep: (e) => j.handleAsync(() => new Promise(e)), makeAsyncFunction(e) {
    return WebAssembly.promising(e);
  } }, $i = (e) => {
    var r = f["_" + e];
    return c(r, "Cannot call unknown function " + e + ", make sure it is exported"), r;
  }, Bi = (e, r) => {
    c(e.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (u(), N).set(e, r);
  }, zi = (e) => {
    var r = oe(e) + 1, t = Qe(r);
    return q(e, t, r), t;
  }, Rt = (e, r, t, n, o) => {
    var a = { string: (S) => {
      var I = 0;
      return S != null && S !== 0 && (I = zi(S)), I;
    }, array: (S) => {
      var I = Qe(S.length);
      return Bi(S, I), I;
    } };
    function s(S) {
      return r === "string" ? V(S) : r === "boolean" ? !!S : S;
    }
    var l = $i(e), d = [], _ = 0;
    if (c(r !== "array", 'Return type should not be "array".'), n)
      for (var v = 0; v < n.length; v++) {
        var g = a[t[v]];
        g ? (_ === 0 && (_ = $r()), d[v] = g(n[v])) : d[v] = n[v];
      }
    var m = l(...d);
    function p(S) {
      return _ !== 0 && Ze(_), s(S);
    }
    var k = o?.async;
    return k ? m.then(p) : (m = p(m), m);
  }, ji = (e, r, t, n) => (...o) => Rt(e, r, t, o, n), Hi = (...e) => St(...e);
  w.init(), i.createPreloadedFile = Fn, i.preloadFile = et, i.staticInit(), c(G.length === 10);
  {
    if (en(), f.noExitRuntime && (nr = f.noExitRuntime), f.preloadPlugins && (Qr = f.preloadPlugins), f.print && (J = f.print), f.printErr && (b = f.printErr), f.wasmBinary && (Ee = f.wasmBinary), Ki(), f.arguments && f.arguments, f.thisProgram && (yr = f.thisProgram), c(typeof f.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), c(typeof f.read > "u", "Module.read option was removed"), c(typeof f.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), c(typeof f.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), c(typeof f.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), c(typeof f.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), c(typeof f.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), c(typeof f.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), f.preInit)
      for (typeof f.preInit == "function" && (f.preInit = [f.preInit]); f.preInit.length > 0; )
        f.preInit.shift()();
    Ce("preInit");
  }
  f.ccall = Rt, f.cwrap = ji, f.UTF8ToString = V, f.stringToUTF8 = q, f.allocateUTF8 = Hi;
  var Vi = ["writeI53ToI64", "writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "convertU32PairToI53", "getTempRet0", "setTempRet0", "withStackSave", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "runMainThreadEmAsm", "jstoi_q", "autoResumeAudioContext", "getDynCaller", "dynCall", "asmjsMangle", "HandleAllocator", "addOnInit", "addOnPostCtor", "addOnPreMain", "addOnExit", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "intArrayToString", "stringToAscii", "registerKeyEventCallback", "maybeCStringToJsString", "findEventTarget", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "registerBatteryEventCallback", "setCanvasElementSizeCallingThread", "setCanvasElementSizeMainThread", "setCanvasElementSize", "getCanvasSizeCallingThread", "getCanvasSizeMainThread", "getCanvasElementSize", "getCallstack", "convertPCtoSourceLocation", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "safeSetTimeout", "setImmediateWrapped", "safeRequestAnimationFrame", "clearImmediateWrapped", "registerPostMainLoop", "registerPreMainLoop", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "isLeapYear", "ydayFromDate", "arraySum", "addDays", "getSocketFromFD", "getSocketAddress", "FS_mkdirTree", "_setNetworkCallback", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "webgl_enable_EXT_polygon_offset_clamp", "webgl_enable_EXT_clip_control", "webgl_enable_WEBGL_polygon_mode", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "emscriptenWebGLGetUniform", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "emscripten_webgl_destroy_context_before_on_calling_thread", "registerWebGlEventCallback", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "allocateUTF8OnStack", "demangle", "stackTrace", "getNativeTypeSize", "getFunctionArgsName", "requireRegisteredType", "createJsInvokerSignature", "PureVirtualError", "getBasestPointer", "registerInheritedInstance", "unregisterInheritedInstance", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "enumReadValueFromPointer", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "flushPendingDeletes", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "count_emval_handles", "getStringOrSymbol", "emval_returnValue", "emval_lookupTypes", "emval_addMethodCaller"];
  Vi.forEach(Zt);
  var Gi = ["run", "out", "err", "callMain", "abort", "wasmExports", "HEAPF32", "HEAPF64", "HEAP8", "HEAP16", "HEAPU16", "HEAP32", "HEAPU32", "HEAP64", "HEAPU64", "writeStackCookie", "checkStackCookie", "INT53_MAX", "INT53_MIN", "bigintToI53Checked", "stackSave", "stackRestore", "stackAlloc", "createNamedFunction", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "ERRNO_CODES", "strError", "DNS", "Protocols", "Sockets", "timers", "warnOnce", "readEmAsmArgsArray", "readEmAsmArgs", "runEmAsmFunction", "getExecutableName", "handleException", "keepRuntimeAlive", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "wasmMemory", "getUniqueRunDependency", "noExitRuntime", "addRunDependency", "removeRunDependency", "addOnPreRun", "addOnPostRun", "freeTableIndexes", "functionsInTableMap", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "stringToUTF8Array", "lengthBytesUTF8", "intArrayFromString", "AsciiToString", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "stringToUTF8OnStack", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "jsStackTrace", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "checkWasiClock", "doReadv", "doWritev", "initRandomFill", "randomFill", "emSetImmediate", "emClearImmediate_deps", "emClearImmediate", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "requestFullscreen", "requestFullScreen", "setCanvasSize", "getUserMedia", "createContext", "getPreloadedImageData__data", "wget", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_preloadFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS_unlink", "FS_createPath", "FS_createDevice", "FS_readFile", "FS", "FS_root", "FS_mounts", "FS_devices", "FS_streams", "FS_nextInode", "FS_nameTable", "FS_currentPath", "FS_initialized", "FS_ignorePermissions", "FS_filesystems", "FS_syncFSRequests", "FS_readFiles", "FS_lookupPath", "FS_getPath", "FS_hashName", "FS_hashAddNode", "FS_hashRemoveNode", "FS_lookupNode", "FS_createNode", "FS_destroyNode", "FS_isRoot", "FS_isMountpoint", "FS_isFile", "FS_isDir", "FS_isLink", "FS_isChrdev", "FS_isBlkdev", "FS_isFIFO", "FS_isSocket", "FS_flagsToPermissionString", "FS_nodePermissions", "FS_mayLookup", "FS_mayCreate", "FS_mayDelete", "FS_mayOpen", "FS_checkOpExists", "FS_nextfd", "FS_getStreamChecked", "FS_getStream", "FS_createStream", "FS_closeStream", "FS_dupStream", "FS_doSetAttr", "FS_chrdev_stream_ops", "FS_major", "FS_minor", "FS_makedev", "FS_registerDevice", "FS_getDevice", "FS_getMounts", "FS_syncfs", "FS_mount", "FS_unmount", "FS_lookup", "FS_mknod", "FS_statfs", "FS_statfsStream", "FS_statfsNode", "FS_create", "FS_mkdir", "FS_mkdev", "FS_symlink", "FS_rename", "FS_rmdir", "FS_readdir", "FS_readlink", "FS_stat", "FS_fstat", "FS_lstat", "FS_doChmod", "FS_chmod", "FS_lchmod", "FS_fchmod", "FS_doChown", "FS_chown", "FS_lchown", "FS_fchown", "FS_doTruncate", "FS_truncate", "FS_ftruncate", "FS_utime", "FS_open", "FS_close", "FS_isClosed", "FS_llseek", "FS_read", "FS_write", "FS_mmap", "FS_msync", "FS_ioctl", "FS_writeFile", "FS_cwd", "FS_chdir", "FS_createDefaultDirectories", "FS_createDefaultDevices", "FS_createSpecialDirectories", "FS_createStandardStreams", "FS_staticInit", "FS_init", "FS_quit", "FS_findObject", "FS_analyzePath", "FS_createFile", "FS_createDataFile", "FS_forceLoadFile", "FS_createLazyFile", "FS_absolutePath", "FS_createFolder", "FS_createLink", "FS_joinPath", "FS_mmapAlloc", "FS_standardizePath", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "GL", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "runAndAbortIfError", "Asyncify", "Fibers", "SDL", "SDL_gfx", "print", "printErr", "jstoi_s", "PThread", "terminateWorker", "cleanupThread", "registerTLSInit", "spawnThread", "exitOnMainThread", "proxyToMainThread", "proxiedJSCallArgs", "invokeEntryPoint", "checkMailbox", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "getTypeName", "getFunctionName", "heap32VectorToArray", "usesDestructorStack", "checkArgCount", "getRequiredArgCount", "createJsInvoker", "UnboundTypeError", "EmValType", "EmValOptionalType", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "embindRepr", "registeredInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "floatReadValueFromPointer", "assertIntegerRange", "readPointer", "runDestructors", "craftInvokerFunction", "embind__requireFunction", "finalizationRegistry", "detachFinalizer_deps", "deletionQueue", "delayFunction", "emval_freelist", "emval_handles", "emval_symbols", "Emval", "emval_methodCallers"];
  Gi.forEach(Sr);
  var qi = [er, Br, qr, rt, tt, nt, it, ot, at, st, gt, yt, Ft, At, Pt, Ct, It, Mt];
  function Ki() {
    Yt("fetchSettings");
  }
  var fr = { 540644: () => typeof wasmOffsetConverter < "u" };
  function Dt() {
    return typeof wasmOffsetConverter < "u";
  }
  Dt.sig = "i";
  var Nt = F("___getTypeName"), Ot = F("__embind_initialize_bindings");
  f._get_cp_model_schema = F("_get_cp_model_schema"), f._get_sat_parameters_schema = F("_get_sat_parameters_schema"), f._solve_model = F("_solve_model");
  var $e = f._malloc = F("_malloc");
  f._free_buffer = F("_free_buffer");
  var K = f._free = F("_free");
  f._interrupt_solve = F("_interrupt_solve"), f._validate_model = F("_validate_model");
  var pe = F("_pthread_self"), _r = F("_fflush"), Wt = F("_strerror"), xt = F("_emscripten_builtin_memalign"), mr = F("__emscripten_thread_init"), Lt = F("__emscripten_thread_crashed"), vr = F("_emscripten_stack_get_end"), Ut = F("__emscripten_run_js_on_main_thread"), $t = F("__emscripten_thread_free_data"), hr = F("__emscripten_thread_exit"), Bt = F("__emscripten_check_mailbox"), zt = F("_emscripten_stack_init"), jt = F("_emscripten_stack_set_limits"), Ht = F("__emscripten_stack_restore"), Vt = F("__emscripten_stack_alloc"), pr = F("_emscripten_stack_get_current"), Gt = F("wasmTable");
  function Xi(e) {
    c(typeof e.__getTypeName < "u", "missing Wasm export: __getTypeName"), c(typeof e._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), c(typeof e.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), c(typeof e.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), c(typeof e.solve_model < "u", "missing Wasm export: solve_model"), c(typeof e.malloc < "u", "missing Wasm export: malloc"), c(typeof e.free_buffer < "u", "missing Wasm export: free_buffer"), c(typeof e.free < "u", "missing Wasm export: free"), c(typeof e.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), c(typeof e.validate_model < "u", "missing Wasm export: validate_model"), c(typeof e.pthread_self < "u", "missing Wasm export: pthread_self"), c(typeof e.fflush < "u", "missing Wasm export: fflush"), c(typeof e.strerror < "u", "missing Wasm export: strerror"), c(typeof e._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), c(typeof e.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), c(typeof e._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), c(typeof e._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), c(typeof e.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), c(typeof e.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), c(typeof e._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), c(typeof e._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), c(typeof e._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), c(typeof e._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), c(typeof e.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), c(typeof e.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), c(typeof e.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), c(typeof e._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), c(typeof e._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), c(typeof e.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), c(typeof e.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), Nt = R("__getTypeName", 1), Ot = R("_embind_initialize_bindings", 0), f._get_cp_model_schema = R("get_cp_model_schema", 0), f._get_sat_parameters_schema = R("get_sat_parameters_schema", 0), f._solve_model = R("solve_model", 5), $e = f._malloc = R("malloc", 1), f._free_buffer = R("free_buffer", 1), K = f._free = R("free", 1), f._interrupt_solve = R("interrupt_solve", 0), f._validate_model = R("validate_model", 3), pe = R("pthread_self", 0), _r = R("fflush", 1), Wt = R("strerror", 1), xt = R("emscripten_builtin_memalign", 2), mr = R("_emscripten_thread_init", 6), Lt = R("_emscripten_thread_crashed", 0), vr = e.emscripten_stack_get_end, e.emscripten_stack_get_base, Ut = R("_emscripten_run_js_on_main_thread", 5), $t = R("_emscripten_thread_free_data", 1), hr = R("_emscripten_thread_exit", 1), Bt = R("_emscripten_check_mailbox", 0), zt = e.emscripten_stack_init, jt = e.emscripten_stack_set_limits, e.emscripten_stack_get_free, Ht = e._emscripten_stack_restore, Vt = e._emscripten_stack_alloc, pr = e.emscripten_stack_get_current, Gt = e.__indirect_function_table;
  }
  var ge;
  function Yi() {
    ge = { HaveOffsetConverter: Dt, __assert_fail: _n, __cxa_throw: vn, __pthread_create_js: Kr, __syscall_fcntl64: rt, __syscall_fstat64: tt, __syscall_ioctl: nt, __syscall_lstat64: it, __syscall_newfstatat: ot, __syscall_openat: at, __syscall_stat64: st, _abort_js: An, _embind_register_bigint: In, _embind_register_bool: Mn, _embind_register_emval: Nn, _embind_register_float: Wn, _embind_register_function: Qn, _embind_register_integer: ei, _embind_register_memory_view: ri, _embind_register_std_string: ti, _embind_register_std_wstring: di, _embind_register_void: ci, _emscripten_init_main_thread_js: ui, _emscripten_notify_mailbox_postmessage: _i, _emscripten_receive_on_main_thread_js: mi, _emscripten_thread_cleanup: vi, _emscripten_thread_mailbox_await: dr, _emscripten_thread_set_strongref: hi, _mmap_js: gt, _munmap_js: yt, _tzset_js: yi, clock_time_get: bi, emscripten_asm_const_int: Ti, emscripten_check_blocking_allowed: Fi, emscripten_errn: Ai, emscripten_exit_with_live_runtime: Pi, emscripten_get_heap_max: Ci, emscripten_get_now: Et, emscripten_num_logical_cores: Ii, emscripten_pc_get_function: Ue, emscripten_resize_heap: Di, emscripten_stack_snapshot: Mi, emscripten_stack_unwind_buffer: Ni, environ_get: Ft, environ_sizes_get: At, exit: rr, fd_close: Pt, fd_read: Ct, fd_seek: It, fd_write: Mt, memory: B, proc_exit: er, random_get: Li };
  }
  var qt;
  function Ji() {
    c(!y), zt(), br();
  }
  function Be() {
    if (te > 0) {
      ke = Be;
      return;
    }
    if (y) {
      Ke?.(f), Pr();
      return;
    }
    if (Ji(), rn(), te > 0) {
      ke = Be;
      return;
    }
    async function e() {
      c(!qt), qt = !0, f.calledRun = !0, !se && (Pr(), Ke?.(f), f.onRuntimeInitialized?.(), Ce("onRuntimeInitialized"), c(!f._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), tn());
    }
    f.setStatus ? (f.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => f.setStatus(""), 1), e();
    }, 1)) : e(), we();
  }
  function Zi() {
    var e = J, r = b, t = !1;
    J = b = (d) => {
      t = !0;
    };
    try {
      _r(0);
      for (var n of ["stdout", "stderr"]) {
        var o = i.analyzePath("/dev/" + n);
        if (!o) return;
        var a = o.object, s = a.rdev, l = Q.ttys[s];
        l?.output?.length && (t = !0);
      }
    } catch {
    }
    J = e, b = r, t && Z("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
  }
  var X;
  y || (X = await Ir(), Be()), de ? je = f : je = new Promise((e, r) => {
    Ke = e, Xe = r;
  });
  for (const e of Object.keys(f))
    e in ze || Object.defineProperty(ze, e, { configurable: !0, get() {
      A(`Access to module property ('${e}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
    } });
  return je;
}
var eo = globalThis.self?.name?.startsWith("em-pthread");
eo && Qi();
//# sourceMappingURL=cp_sat_runtime-CFj6urGq.js.map
