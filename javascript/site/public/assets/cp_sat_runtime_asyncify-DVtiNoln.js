async function oa(He = {}) {
  var Ve;
  (function() {
    function e(c) {
      c = c.split("-")[0];
      for (var _ = c.split(".").slice(0, 3); _.length < 3; ) _.push("00");
      return _ = _.map((h, w, p) => h.padStart(2, "0")), _.join("");
    }
    var r = (c) => [c / 1e4 | 0, (c / 100 | 0) % 100, c % 100].join("."), t = 2147483647, n = typeof process < "u" && process.versions?.node ? e(process.versions.node) : t;
    if (n < t)
      throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    if (n < 2147483647)
      throw new Error(`This emscripten-generated code requires node v${r(2147483647)} (detected v${r(n)})`);
    var a = typeof navigator < "u" && navigator.userAgent;
    if (a) {
      var o = a.includes("Safari/") && a.match(/Version\/(\d+\.?\d*\.?\d*)/) ? e(a.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : t;
      if (o < 15e4)
        throw new Error(`This emscripten-generated code requires Safari v${r(15e4)} (detected v${o})`);
      var s = a.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(a.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : t;
      if (s < 114)
        throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${s})`);
      var l = a.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(a.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : t;
      if (l < 85)
        throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${l})`);
    }
  })();
  var f = He, Pe = !!globalThis.window, Z = !!globalThis.WorkerGlobalScope, Ge = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", Er = !Pe && !Ge && !Z, S = Z && self.name?.startsWith("em-pthread");
  S && (d(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0);
  var Sr = "./this.program", br = (e, r) => {
    throw r;
  }, Zt = import.meta.url, qe = "";
  function Qt(e) {
    return f.locateFile ? f.locateFile(e, qe) : qe + e;
  }
  var Ke, Ie;
  if (!Er) if (Pe || Z) {
    try {
      qe = new URL(".", Zt).href;
    } catch {
    }
    if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    Z && (Ie = (e) => {
      var r = new XMLHttpRequest();
      return r.open("GET", e, !1), r.responseType = "arraybuffer", r.send(null), new Uint8Array(r.response);
    }), Ke = async (e) => {
      d(!kr(e), "readAsync does not work with file:// URLs");
      var r = await fetch(e, { credentials: "same-origin" });
      if (r.ok)
        return r.arrayBuffer();
      throw new Error(r.status + " : " + r.url);
    };
  } else
    throw new Error("environment detection error");
  var Q = console.log.bind(console), k = console.error.bind(console);
  d(Pe || Z || Ge, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), d(!Ge, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable."), d(!Er, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
  var Ee;
  globalThis.WebAssembly || k("no native wasm support detected");
  var te, q = !1, de;
  function d(e, r) {
    e || P("Assertion failed" + (r ? ": " + r : ""));
  }
  var kr = (e) => e.startsWith("file://");
  function Tr() {
    var e = yr();
    d((e & 3) == 0), e == 0 && (e += 4), (u(), y)[e >> 2] = 34821223, (u(), y)[e + 4 >> 2] = 2310721022, (u(), y)[0] = 1668509029;
  }
  function Se() {
    if (!q) {
      var e = yr();
      e == 0 && (e += 4);
      var r = (u(), y)[e >> 2], t = (u(), y)[e + 4 >> 2];
      (r != 34821223 || t != 2310721022) && P(`Stack overflow! Stack cookie has been overwritten at ${oe(e)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${oe(t)} ${oe(r)}`), (u(), y)[0] != 1668509029 && P("Runtime error: The application has corrupted its heap memory area (address zero)!");
    }
  }
  function Xe(...e) {
    console.warn(...e);
  }
  (() => {
    var e = new Int16Array(1), r = new Int8Array(e.buffer);
    e[0] = 25459, (r[0] !== 115 || r[1] !== 99) && P("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
  })();
  function De(e) {
    Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, { configurable: !0, set() {
      P(`Attempt to set \`Module.${e}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
    } });
  }
  function C(e) {
    return () => d(!1, `call to '${e}' via reference taken before Wasm module initialization`);
  }
  function en(e) {
    Object.getOwnPropertyDescriptor(f, e) && P(`\`Module.${e}\` was supplied but \`${e}\` not included in INCOMING_MODULE_JS_API`);
  }
  function rn(e) {
    return e === "FS_createPath" || e === "FS_createDataFile" || e === "FS_createPreloadedFile" || e === "FS_preloadFile" || e === "FS_unlink" || e === "addRunDependency" || e === "FS_createLazyFile" || e === "FS_createDevice" || e === "removeRunDependency";
  }
  function tn(e) {
    Fr(e);
  }
  function Fr(e) {
    S || Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, { configurable: !0, get() {
      var r = `'${e}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
      rn(e) && (r += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), P(r);
    } });
  }
  function nn() {
    function e() {
      var t = 0;
      return ce && typeof ye < "u" && (t = ye()), `w:${Cr},t:${oe(t)}:`;
    }
    var r = Xe;
    Xe = (...t) => r(e(), ...t);
  }
  nn();
  function u() {
    z.buffer != L.buffer && Re();
  }
  var Ye, Je, Cr = 0, Ar;
  if (S) {
    let e = function(r) {
      try {
        var t = r.data, n = t.cmd;
        if (n === "load") {
          Cr = t.workerID;
          let a = [];
          self.onmessage = (o) => a.push(o), Ar = () => {
            postMessage({ cmd: "loaded" });
            for (let o of a)
              e(o);
            self.onmessage = e;
          };
          for (const o of t.handlers)
            (!f[o] || f[o].proxy) && (f[o] = (...s) => {
              postMessage({ cmd: "callHandler", handler: o, args: s });
            }, o == "print" && (Q = f[o]), o == "printErr" && (k = f[o]));
          z = t.wasmMemory, Re(), te = t.wasmModule, Mr(), ze();
        } else if (n === "run") {
          d(t.pthread_ptr), hn(t.pthread_ptr), hr(t.pthread_ptr, 0, 0, 1, 0, 0), T.threadInitTLS(), fr(t.pthread_ptr), Ze || (Ot(), Ze = !0);
          try {
            Gr(t.start_routine, t.arg);
          } catch (a) {
            if (a != "unwind")
              throw a;
          }
        } else t.target === "setimmediate" || (n === "checkMailbox" ? Ze && xe() : n && (k(`worker: received unknown command ${n}`), k(t)));
      } catch (a) {
        throw k(`worker: onmessage() captured an uncaught exception: ${a}`), a?.stack && k(a.stack), xt(), a;
      }
    };
    var Ze = !1;
    self.onunhandledrejection = (r) => {
      throw r.reason || r;
    }, self.onmessage = e;
  }
  var L, U, ne, be, M, y, Pr, ke, O, Ir, ce = !1;
  function Re() {
    var e = z.buffer;
    L = new Int8Array(e), ne = new Int16Array(e), f.HEAPU8 = U = new Uint8Array(e), be = new Uint16Array(e), M = new Int32Array(e), y = new Uint32Array(e), Pr = new Float32Array(e), ke = new Float64Array(e), O = new BigInt64Array(e), Ir = new BigUint64Array(e);
  }
  function an() {
    if (!S) {
      if (f.wasmMemory)
        z = f.wasmMemory;
      else {
        var e = f.INITIAL_MEMORY || 16777216;
        d(e >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + e + "! (STACK_SIZE=65536)"), z = new WebAssembly.Memory({ initial: e / 65536, maximum: 32768, shared: !0 });
      }
      Re();
    }
  }
  d(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
  function on() {
    if (d(!S), f.preRun)
      for (typeof f.preRun == "function" && (f.preRun = [f.preRun]); f.preRun.length; )
        jr(f.preRun.shift());
    De("preRun"), Lr(xr);
  }
  function Dr() {
    if (d(!ce), ce = !0, S) return Ar();
    Se(), !f.noFSInit && !i.initialized && i.init(), J.__wasm_call_ctors(), i.ignorePermissions = !1;
  }
  function sn() {
    if (Se(), !S) {
      if (f.postRun)
        for (typeof f.postRun == "function" && (f.postRun = [f.postRun]); f.postRun.length; )
          mn(f.postRun.shift());
      De("postRun"), Lr(Vr);
    }
  }
  function P(e) {
    f.onAbort?.(e), e = "Aborted(" + e + ")", k(e), q = !0, e.indexOf("RuntimeError: unreachable") >= 0 && (e += '. "unreachable" may be due to ASYNCIFY_STACK_SIZE not being large enough (try increasing it)');
    var r = new WebAssembly.RuntimeError(e);
    throw Je?.(r), r;
  }
  function v(e, r) {
    return (...t) => {
      d(ce, `native function \`${e}\` called before runtime initialization`);
      var n = J[e];
      return d(n, `exported native function \`${e}\` not found`), d(t.length <= r, `native function \`${e}\` called with ${t.length} args but expects ${r}`), n(...t);
    };
  }
  var Qe;
  function ln() {
    return f.locateFile ? Qt("cp_sat_runtime_asyncify.wasm") : new URL("/assets/cp_sat_runtime_asyncify-DMLrX9Z5.wasm", import.meta.url).href;
  }
  function dn(e) {
    if (e == Qe && Ee)
      return new Uint8Array(Ee);
    if (Ie)
      return Ie(e);
    throw "both async and sync fetching of the wasm failed";
  }
  async function cn(e) {
    if (!Ee)
      try {
        var r = await Ke(e);
        return new Uint8Array(r);
      } catch {
      }
    return dn(e);
  }
  async function un(e, r) {
    try {
      var t = await cn(e), n = await WebAssembly.instantiate(t, r);
      return n;
    } catch (a) {
      k(`failed to asynchronously prepare wasm: ${a}`), kr(e) && k(`warning: Loading from a file URI (${e}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), P(a);
    }
  }
  async function fn(e, r, t) {
    if (!e)
      try {
        var n = fetch(r, { credentials: "same-origin" }), a = await WebAssembly.instantiateStreaming(n, t);
        return a;
      } catch (o) {
        k(`wasm streaming compile failed: ${o}`), k("falling back to ArrayBuffer instantiation");
      }
    return un(r, t);
  }
  function Rr() {
    na(), ge.__instrumented || (ge.__instrumented = !0, m.instrumentWasmImports(ge));
    var e = { env: ge, wasi_snapshot_preview1: ge };
    return e;
  }
  async function Mr() {
    function e(l, c) {
      return J = l.exports, J = m.instrumentWasmExports(J), yn(J._emscripten_tls_init), ta(J), te = c, J;
    }
    var r = f;
    function t(l) {
      return d(f === r, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), r = null, e(l.instance, l.module);
    }
    var n = Rr();
    if (f.instantiateWasm)
      return new Promise((l, c) => {
        try {
          f.instantiateWasm(n, (_, h) => {
            l(e(_, h));
          });
        } catch (_) {
          k(`Module.instantiateWasm callback failed with error: ${_}`), c(_);
        }
      });
    if (S) {
      d(te, "wasmModule should have been received via postMessage");
      var a = new WebAssembly.Instance(te, Rr());
      return e(a, te);
    }
    Qe ??= ln();
    var o = await fn(Ee, Qe, n), s = t(o);
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
      k(`received "${t}" command from terminated worker: ${e.workerID}`);
    };
  }, Wr = (e) => {
    d(!S, "Internal Error! cleanupThread() can only ever be called from main application thread!"), d(e, "Internal Error! Null pthread_ptr in cleanupThread!");
    var r = T.pthreads[e];
    d(r), T.returnWorkerToPool(r);
  }, Lr = (e) => {
    for (; e.length > 0; )
      e.shift()(f);
  }, xr = [], jr = (e) => xr.push(e), ie = 0, Te = null, ue = {}, ae = null, Ur = (e) => {
    if (ie--, f.monitorRunDependencies?.(ie), d(e, "removeRunDependency requires an ID"), d(ue[e]), delete ue[e], ie == 0 && (ae !== null && (clearInterval(ae), ae = null), Te)) {
      var r = Te;
      Te = null, r();
    }
  }, $r = (e) => {
    ie++, f.monitorRunDependencies?.(ie), d(e, "addRunDependency requires an ID"), d(!ue[e]), ue[e] = 1, ae === null && globalThis.setInterval && (ae = setInterval(() => {
      if (q) {
        clearInterval(ae), ae = null;
        return;
      }
      var r = !1;
      for (var t in ue)
        r || (r = !0, k("still waiting on run dependencies:")), k(`dependency: ${t}`);
      r && k("(end of list)");
    }, 1e4));
  }, Br = (e) => {
    d(!S, "Internal Error! spawnThread() can only ever be called from main application thread!"), d(e.pthread_ptr, "Internal error, no pthread ptr!");
    var r = T.getNewWorker();
    if (!r)
      return 6;
    d(!r.pthread_ptr, "Internal error!"), T.runningWorkers.push(r), T.pthreads[e.pthread_ptr] = r, r.pthread_ptr = e.pthread_ptr;
    var t = { cmd: "run", start_routine: e.startRoutine, arg: e.arg, pthread_ptr: e.pthread_ptr };
    return r.postMessage(t, e.transferList), 0;
  }, fe = 0, Me = () => ir || fe > 0, zr = () => wr(), er = (e) => Ht(e), rr = (e) => Vt(e), W = (e, r, t, ...n) => {
    for (var a = n.length * 2, o = zr(), s = rr(a * 8), l = s >> 3, c = 0; c < n.length; c++) {
      var _ = n[c];
      typeof _ == "bigint" ? ((u(), O)[l + 2 * c] = 1n, (u(), O)[l + 2 * c + 1] = _) : ((u(), O)[l + 2 * c] = 0n, (u(), ke)[l + 2 * c + 1] = _);
    }
    var h = jt(e, r, a, s, t);
    return er(o), h;
  };
  function tr(e) {
    if (S) return W(0, 0, 1, e);
    de = e, Me() || (T.terminateAllThreads(), f.onExit?.(e), q = !0), br(e, new Nr(e));
  }
  function Hr(e) {
    if (S) return W(1, 0, 0, e);
    nr(e);
  }
  var _n = (e, r) => {
    if (de = e, aa(), S)
      throw d(!r), Hr(e), "unwind";
    if (Me() && !r) {
      var t = `program exited (with status: ${e}), but keepRuntimeAlive() is set (counter=${fe}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
      Je?.(t), k(t);
    }
    tr(e);
  }, nr = _n, oe = (e) => (d(typeof e == "number", `ptrToString expects a number, got ${typeof e}`), e >>>= 0, "0x" + e.toString(16).padStart(8, "0")), T = { unusedWorkers: [], runningWorkers: [], tlsInitFunctions: [], pthreads: {}, nextWorkerID: 1, init() {
    S || T.initMainThread();
  }, initMainThread() {
    for (var e = navigator.hardwareConcurrency; e--; )
      T.allocateUnusedWorker();
    jr(async () => {
      var r = T.loadWasmModuleToAllWorkers();
      $r("loading-workers"), await r, Ur("loading-workers");
    });
  }, terminateAllThreads: () => {
    d(!S, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
    for (var e of T.runningWorkers)
      Or(e);
    for (var e of T.unusedWorkers)
      Or(e);
    T.unusedWorkers = [], T.runningWorkers = [], T.pthreads = {};
  }, returnWorkerToPool: (e) => {
    var r = e.pthread_ptr;
    delete T.pthreads[r], T.unusedWorkers.push(e), T.runningWorkers.splice(T.runningWorkers.indexOf(e), 1), e.pthread_ptr = 0, Ut(r);
  }, threadInitTLS() {
    T.tlsInitFunctions.forEach((e) => e());
  }, loadWasmModuleToWorker: (e) => new Promise((r) => {
    e.onmessage = (o) => {
      var s = o.data, l = s.cmd;
      if (s.targetThread && s.targetThread != ye()) {
        var c = T.pthreads[s.targetThread];
        c ? c.postMessage(s, s.transferList) : k(`Internal error! Worker sent a message "${l}" to target pthread ${s.targetThread}, but that thread no longer exists!`);
        return;
      }
      l === "checkMailbox" ? xe() : l === "spawnThread" ? Br(s) : l === "cleanupThread" ? cr(() => Wr(s.thread)) : l === "loaded" ? (e.loaded = !0, r(e)) : s.target === "setimmediate" ? e.postMessage(s) : l === "callHandler" ? f[s.handler](...s.args) : l && k(`worker sent an unknown command ${l}`);
    }, e.onerror = (o) => {
      var s = "worker sent an error!";
      throw e.pthread_ptr && (s = `Pthread ${oe(e.pthread_ptr)} sent an error!`), k(`${s} ${o.filename}:${o.lineno}: ${o.message}`), o;
    }, d(z instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), d(te instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
    var t = [], n = ["onExit", "onAbort", "print", "printErr"];
    for (var a of n)
      f.propertyIsEnumerable(a) && t.push(a);
    e.postMessage({ cmd: "load", handlers: t, wasmMemory: z, wasmModule: te, workerID: e.workerID });
  }), async loadWasmModuleToAllWorkers() {
    return S ? void 0 : Promise.all(T.unusedWorkers.map(T.loadWasmModuleToWorker));
  }, allocateUnusedWorker() {
    var e;
    if (f.mainScriptUrlOrBlob) {
      var r = f.mainScriptUrlOrBlob;
      typeof r != "string" && (r = URL.createObjectURL(r)), e = new Worker(
        r,
        /* @vite-ignore */
        { type: "module", name: "em-pthread-" + T.nextWorkerID }
      );
    } else e = new Worker(self.location.href, { type: "module", name: "em-pthread-" + T.nextWorkerID });
    e.workerID = T.nextWorkerID++, T.unusedWorkers.push(e);
  }, getNewWorker() {
    if (T.unusedWorkers.length == 0) {
      k("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.");
      return;
    }
    return T.unusedWorkers.pop();
  } }, Vr = [], mn = (e) => Vr.push(e), E = {}, vn = (e, r, t) => {
    e = e.replace(/p/g, "i"), d(e in E, `bad function pointer type - sig is not in dynCalls: '${e}'`), t?.length ? d(t.length === e.length - 1) : d(e.length == 1);
    var n = E[e];
    return n(r, ...t);
  }, pn = (e, r, t = [], n = !1) => {
    d(r, "null function pointer in dynCall"), d(!n, "async dynCall is not supported in this mode");
    var a = vn(e, r, t);
    function o(s) {
      return s;
    }
    return a;
  };
  function hn(e) {
    var r = (u(), y)[e + 52 >> 2], t = (u(), y)[e + 56 >> 2], n = r - t;
    d(r != 0), d(n != 0), d(r > n, "stackHigh must be higher then stackLow"), zt(r, n), er(r), Tr();
  }
  var Gr = (e, r) => {
    fe = 0, ir = 0;
    var t = ((a) => Gt(e, a))(r);
    Se();
    function n(a) {
      if (Me()) {
        de = a;
        return;
      }
      gr(a);
    }
    n(t);
  };
  Gr.isAsync = !0;
  var ir = !0, yn = (e) => T.tlsInitFunctions.push(e), ee = (e) => {
    ee.shown ||= {}, ee.shown[e] || (ee.shown[e] = 1, k(e));
  }, z, qr = globalThis.TextDecoder && new TextDecoder(), Kr = (e, r, t, n) => {
    var a = r + t;
    if (n) return a;
    for (; e[r] && !(r >= a); ) ++r;
    return r;
  }, _e = (e, r = 0, t, n) => {
    var a = Kr(e, r, t, n);
    if (a - r > 16 && e.buffer && qr)
      return qr.decode(e.buffer instanceof ArrayBuffer ? e.subarray(r, a) : e.slice(r, a));
    for (var o = ""; r < a; ) {
      var s = e[r++];
      if (!(s & 128)) {
        o += String.fromCharCode(s);
        continue;
      }
      var l = e[r++] & 63;
      if ((s & 224) == 192) {
        o += String.fromCharCode((s & 31) << 6 | l);
        continue;
      }
      var c = e[r++] & 63;
      if ((s & 240) == 224 ? s = (s & 15) << 12 | l << 6 | c : ((s & 248) != 240 && ee("Invalid UTF-8 leading byte " + oe(s) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), s = (s & 7) << 18 | l << 12 | c << 6 | e[r++] & 63), s < 65536)
        o += String.fromCharCode(s);
      else {
        var _ = s - 65536;
        o += String.fromCharCode(55296 | _ >> 10, 56320 | _ & 1023);
      }
    }
    return o;
  }, K = (e, r, t) => (d(typeof e == "number", `UTF8ToString expects a number (got ${typeof e})`), e ? _e((u(), U), e, r, t) : ""), gn = (e, r, t, n) => P(`Assertion failed: ${K(e)}, at: ` + [r ? K(r) : "unknown filename", t, n ? K(n) : "unknown function"]);
  class wn {
    constructor(r) {
      this.excPtr = r, this.ptr = r - 24;
    }
    set_type(r) {
      (u(), y)[this.ptr + 4 >> 2] = r;
    }
    get_type() {
      return (u(), y)[this.ptr + 4 >> 2];
    }
    set_destructor(r) {
      (u(), y)[this.ptr + 8 >> 2] = r;
    }
    get_destructor() {
      return (u(), y)[this.ptr + 8 >> 2];
    }
    set_caught(r) {
      r = r ? 1 : 0, (u(), L)[this.ptr + 12] = r;
    }
    get_caught() {
      return (u(), L)[this.ptr + 12] != 0;
    }
    set_rethrown(r) {
      r = r ? 1 : 0, (u(), L)[this.ptr + 13] = r;
    }
    get_rethrown() {
      return (u(), L)[this.ptr + 13] != 0;
    }
    init(r, t) {
      this.set_adjusted_ptr(0), this.set_type(r), this.set_destructor(t);
    }
    set_adjusted_ptr(r) {
      (u(), y)[this.ptr + 16 >> 2] = r;
    }
    get_adjusted_ptr() {
      return (u(), y)[this.ptr + 16 >> 2];
    }
  }
  var En = (e, r, t) => {
    var n = new wn(e);
    n.init(r, t), d(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
  };
  function Xr(e, r, t, n) {
    return S ? W(2, 0, 1, e, r, t, n) : Yr(e, r, t, n);
  }
  var Sn = () => !!globalThis.SharedArrayBuffer, Yr = (e, r, t, n) => {
    if (!Sn())
      return Xe("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
    var a = [], o = 0;
    if (S && (a.length === 0 || o))
      return Xr(e, r, t, n);
    var s = { startRoutine: t, pthread_ptr: e, arg: n, transferList: a };
    return S ? (s.cmd = "spawnThread", postMessage(s, a), 0) : Br(s);
  }, Ne = () => {
    d(D.varargs != null);
    var e = (u(), M)[+D.varargs >> 2];
    return D.varargs += 4, e;
  }, me = Ne, I = { isAbs: (e) => e.charAt(0) === "/", splitPath: (e) => {
    var r = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
    return r.exec(e).slice(1);
  }, normalizeArray: (e, r) => {
    for (var t = 0, n = e.length - 1; n >= 0; n--) {
      var a = e[n];
      a === "." ? e.splice(n, 1) : a === ".." ? (e.splice(n, 1), t++) : t && (e.splice(n, 1), t--);
    }
    if (r)
      for (; t; t--)
        e.unshift("..");
    return e;
  }, normalize: (e) => {
    var r = I.isAbs(e), t = e.slice(-1) === "/";
    return e = I.normalizeArray(e.split("/").filter((n) => !!n), !r).join("/"), !e && !r && (e = "."), e && t && (e += "/"), (r ? "/" : "") + e;
  }, dirname: (e) => {
    var r = I.splitPath(e), t = r[0], n = r[1];
    return !t && !n ? "." : (n && (n = n.slice(0, -1)), t + n);
  }, basename: (e) => e && e.match(/([^\/]+|\/)\/*$/)[1], join: (...e) => I.normalize(e.join("/")), join2: (e, r) => I.normalize(e + "/" + r) }, bn = () => (e) => e.set(crypto.getRandomValues(new Uint8Array(e.byteLength))), ar = (e) => {
    (ar = bn())(e);
  }, ve = { resolve: (...e) => {
    for (var r = "", t = !1, n = e.length - 1; n >= -1 && !t; n--) {
      var a = n >= 0 ? e[n] : i.cwd();
      if (typeof a != "string")
        throw new TypeError("Arguments to path.resolve must be strings");
      if (!a)
        return "";
      r = a + "/" + r, t = I.isAbs(a);
    }
    return r = I.normalizeArray(r.split("/").filter((o) => !!o), !t).join("/"), (t ? "/" : "") + r || ".";
  }, relative: (e, r) => {
    e = ve.resolve(e).slice(1), r = ve.resolve(r).slice(1);
    function t(_) {
      for (var h = 0; h < _.length && _[h] === ""; h++)
        ;
      for (var w = _.length - 1; w >= 0 && _[w] === ""; w--)
        ;
      return h > w ? [] : _.slice(h, w - h + 1);
    }
    for (var n = t(e.split("/")), a = t(r.split("/")), o = Math.min(n.length, a.length), s = o, l = 0; l < o; l++)
      if (n[l] !== a[l]) {
        s = l;
        break;
      }
    for (var c = [], l = s; l < n.length; l++)
      c.push("..");
    return c = c.concat(a.slice(s)), c.join("/");
  } }, or = [], se = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.charCodeAt(t);
      n <= 127 ? r++ : n <= 2047 ? r += 2 : n >= 55296 && n <= 57343 ? (r += 4, ++t) : r += 3;
    }
    return r;
  }, Jr = (e, r, t, n) => {
    if (d(typeof e == "string", `stringToUTF8Array expects a string (got ${typeof e})`), !(n > 0)) return 0;
    for (var a = t, o = t + n - 1, s = 0; s < e.length; ++s) {
      var l = e.codePointAt(s);
      if (l <= 127) {
        if (t >= o) break;
        r[t++] = l;
      } else if (l <= 2047) {
        if (t + 1 >= o) break;
        r[t++] = 192 | l >> 6, r[t++] = 128 | l & 63;
      } else if (l <= 65535) {
        if (t + 2 >= o) break;
        r[t++] = 224 | l >> 12, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63;
      } else {
        if (t + 3 >= o) break;
        l > 1114111 && ee("Invalid Unicode code point " + oe(l) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), r[t++] = 240 | l >> 18, r[t++] = 128 | l >> 12 & 63, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63, s++;
      }
    }
    return r[t] = 0, t - a;
  }, sr = (e, r, t) => {
    var n = se(e) + 1, a = new Array(n), o = Jr(e, a, 0, a.length);
    return a.length = o, a;
  }, kn = () => {
    if (!or.length) {
      var e = null;
      if (globalThis.window?.prompt && (e = window.prompt("Input: "), e !== null && (e += `
`)), !e)
        return null;
      or = sr(e);
    }
    return or.shift();
  }, re = { ttys: [], init() {
  }, shutdown() {
  }, register(e, r) {
    re.ttys[e] = { input: [], output: [], ops: r }, i.registerDevice(e, re.stream_ops);
  }, stream_ops: { open(e) {
    var r = re.ttys[e.node.rdev];
    if (!r)
      throw new i.ErrnoError(43);
    e.tty = r, e.seekable = !1;
  }, close(e) {
    e.tty.ops.fsync(e.tty);
  }, fsync(e) {
    e.tty.ops.fsync(e.tty);
  }, read(e, r, t, n, a) {
    if (!e.tty || !e.tty.ops.get_char)
      throw new i.ErrnoError(60);
    for (var o = 0, s = 0; s < n; s++) {
      var l;
      try {
        l = e.tty.ops.get_char(e.tty);
      } catch {
        throw new i.ErrnoError(29);
      }
      if (l === void 0 && o === 0)
        throw new i.ErrnoError(6);
      if (l == null) break;
      o++, r[t + s] = l;
    }
    return o && (e.node.atime = Date.now()), o;
  }, write(e, r, t, n, a) {
    if (!e.tty || !e.tty.ops.put_char)
      throw new i.ErrnoError(60);
    try {
      for (var o = 0; o < n; o++)
        e.tty.ops.put_char(e.tty, r[t + o]);
    } catch {
      throw new i.ErrnoError(29);
    }
    return n && (e.node.mtime = e.node.ctime = Date.now()), o;
  } }, default_tty_ops: { get_char(e) {
    return kn();
  }, put_char(e, r) {
    r === null || r === 10 ? (Q(_e(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (Q(_e(e.output)), e.output = []);
  }, ioctl_tcgets(e) {
    return { c_iflag: 25856, c_oflag: 5, c_cflag: 191, c_lflag: 35387, c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  }, ioctl_tcsets(e, r, t) {
    return 0;
  }, ioctl_tiocgwinsz(e) {
    return [24, 80];
  } }, default_tty1_ops: { put_char(e, r) {
    r === null || r === 10 ? (k(_e(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (k(_e(e.output)), e.output = []);
  } } }, Tn = (e, r) => (u(), U).fill(0, e, e + r), Zr = (e, r) => (d(r, "alignment argument is required"), Math.ceil(e / r) * r), Qr = (e) => {
    e = Zr(e, 65536);
    var r = Lt(65536, e);
    return r && Tn(r, e), r;
  }, b = { ops_table: null, mount(e) {
    return b.createNode(null, "/", 16895, 0);
  }, createNode(e, r, t, n) {
    if (i.isBlkdev(t) || i.isFIFO(t))
      throw new i.ErrnoError(63);
    b.ops_table ||= { dir: { node: { getattr: b.node_ops.getattr, setattr: b.node_ops.setattr, lookup: b.node_ops.lookup, mknod: b.node_ops.mknod, rename: b.node_ops.rename, unlink: b.node_ops.unlink, rmdir: b.node_ops.rmdir, readdir: b.node_ops.readdir, symlink: b.node_ops.symlink }, stream: { llseek: b.stream_ops.llseek } }, file: { node: { getattr: b.node_ops.getattr, setattr: b.node_ops.setattr }, stream: { llseek: b.stream_ops.llseek, read: b.stream_ops.read, write: b.stream_ops.write, mmap: b.stream_ops.mmap, msync: b.stream_ops.msync } }, link: { node: { getattr: b.node_ops.getattr, setattr: b.node_ops.setattr, readlink: b.node_ops.readlink }, stream: {} }, chrdev: { node: { getattr: b.node_ops.getattr, setattr: b.node_ops.setattr }, stream: i.chrdev_stream_ops } };
    var a = i.createNode(e, r, t, n);
    return i.isDir(a.mode) ? (a.node_ops = b.ops_table.dir.node, a.stream_ops = b.ops_table.dir.stream, a.contents = {}) : i.isFile(a.mode) ? (a.node_ops = b.ops_table.file.node, a.stream_ops = b.ops_table.file.stream, a.usedBytes = 0, a.contents = null) : i.isLink(a.mode) ? (a.node_ops = b.ops_table.link.node, a.stream_ops = b.ops_table.link.stream) : i.isChrdev(a.mode) && (a.node_ops = b.ops_table.chrdev.node, a.stream_ops = b.ops_table.chrdev.stream), a.atime = a.mtime = a.ctime = Date.now(), e && (e.contents[r] = a, e.atime = e.mtime = e.ctime = a.atime), a;
  }, getFileDataAsTypedArray(e) {
    return e.contents ? e.contents.subarray ? e.contents.subarray(0, e.usedBytes) : new Uint8Array(e.contents) : new Uint8Array(0);
  }, expandFileStorage(e, r) {
    var t = e.contents ? e.contents.length : 0;
    if (!(t >= r)) {
      var n = 1024 * 1024;
      r = Math.max(r, t * (t < n ? 2 : 1.125) >>> 0), t != 0 && (r = Math.max(r, 256));
      var a = e.contents;
      e.contents = new Uint8Array(r), e.usedBytes > 0 && e.contents.set(a.subarray(0, e.usedBytes), 0);
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
    r.size !== void 0 && b.resizeFileStorage(e, r.size);
  }, lookup(e, r) {
    throw new i.ErrnoError(44);
  }, mknod(e, r, t, n) {
    return b.createNode(e, r, t, n);
  }, rename(e, r, t) {
    var n;
    try {
      n = i.lookupNode(r, t);
    } catch {
    }
    if (n) {
      if (i.isDir(e.mode))
        for (var a in n.contents)
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
    var n = b.createNode(e, r, 41471, 0);
    return n.link = t, n;
  }, readlink(e) {
    if (!i.isLink(e.mode))
      throw new i.ErrnoError(28);
    return e.link;
  } }, stream_ops: { read(e, r, t, n, a) {
    var o = e.node.contents;
    if (a >= e.node.usedBytes) return 0;
    var s = Math.min(e.node.usedBytes - a, n);
    if (d(s >= 0), s > 8 && o.subarray)
      r.set(o.subarray(a, a + s), t);
    else
      for (var l = 0; l < s; l++) r[t + l] = o[a + l];
    return s;
  }, write(e, r, t, n, a, o) {
    if (d(!(r instanceof ArrayBuffer)), r.buffer === (u(), L).buffer && (o = !1), !n) return 0;
    var s = e.node;
    if (s.mtime = s.ctime = Date.now(), r.subarray && (!s.contents || s.contents.subarray)) {
      if (o)
        return d(a === 0, "canOwn must imply no weird position inside the file"), s.contents = r.subarray(t, t + n), s.usedBytes = n, n;
      if (s.usedBytes === 0 && a === 0)
        return s.contents = r.slice(t, t + n), s.usedBytes = n, n;
      if (a + n <= s.usedBytes)
        return s.contents.set(r.subarray(t, t + n), a), n;
    }
    if (b.expandFileStorage(s, a + n), s.contents.subarray && r.subarray)
      s.contents.set(r.subarray(t, t + n), a);
    else
      for (var l = 0; l < n; l++)
        s.contents[a + l] = r[t + l];
    return s.usedBytes = Math.max(s.usedBytes, a + n), n;
  }, llseek(e, r, t) {
    var n = r;
    if (t === 1 ? n += e.position : t === 2 && i.isFile(e.node.mode) && (n += e.node.usedBytes), n < 0)
      throw new i.ErrnoError(28);
    return n;
  }, mmap(e, r, t, n, a) {
    if (!i.isFile(e.node.mode))
      throw new i.ErrnoError(43);
    var o, s, l = e.node.contents;
    if (!(a & 2) && l && l.buffer === (u(), L).buffer)
      s = !1, o = l.byteOffset;
    else {
      if (s = !0, o = Qr(r), !o)
        throw new i.ErrnoError(48);
      l && ((t > 0 || t + r < l.length) && (l.subarray ? l = l.subarray(t, t + r) : l = Array.prototype.slice.call(l, t, t + r)), (u(), L).set(l, o));
    }
    return { ptr: o, allocated: s };
  }, msync(e, r, t, n, a) {
    return b.stream_ops.write(e, r, 0, n, t, !1), 0;
  } } }, Fn = (e) => {
    var r = { r: 0, "r+": 2, w: 577, "w+": 578, a: 1089, "a+": 1090 }, t = r[e];
    if (typeof t > "u")
      throw new Error(`Unknown file open mode: ${e}`);
    return t;
  }, lr = (e, r) => {
    var t = 0;
    return e && (t |= 365), r && (t |= 146), t;
  }, Cn = (e) => K(Wt(e)), et = { EPERM: 63, ENOENT: 44, ESRCH: 71, EINTR: 27, EIO: 29, ENXIO: 60, E2BIG: 1, ENOEXEC: 45, EBADF: 8, ECHILD: 12, EAGAIN: 6, EWOULDBLOCK: 6, ENOMEM: 48, EACCES: 2, EFAULT: 21, ENOTBLK: 105, EBUSY: 10, EEXIST: 20, EXDEV: 75, ENODEV: 43, ENOTDIR: 54, EISDIR: 31, EINVAL: 28, ENFILE: 41, EMFILE: 33, ENOTTY: 59, ETXTBSY: 74, EFBIG: 22, ENOSPC: 51, ESPIPE: 70, EROFS: 69, EMLINK: 34, EPIPE: 64, EDOM: 18, ERANGE: 68, ENOMSG: 49, EIDRM: 24, ECHRNG: 106, EL2NSYNC: 156, EL3HLT: 107, EL3RST: 108, ELNRNG: 109, EUNATCH: 110, ENOCSI: 111, EL2HLT: 112, EDEADLK: 16, ENOLCK: 46, EBADE: 113, EBADR: 114, EXFULL: 115, ENOANO: 104, EBADRQC: 103, EBADSLT: 102, EDEADLOCK: 16, EBFONT: 101, ENOSTR: 100, ENODATA: 116, ETIME: 117, ENOSR: 118, ENONET: 119, ENOPKG: 120, EREMOTE: 121, ENOLINK: 47, EADV: 122, ESRMNT: 123, ECOMM: 124, EPROTO: 65, EMULTIHOP: 36, EDOTDOT: 125, EBADMSG: 9, ENOTUNIQ: 126, EBADFD: 127, EREMCHG: 128, ELIBACC: 129, ELIBBAD: 130, ELIBSCN: 131, ELIBMAX: 132, ELIBEXEC: 133, ENOSYS: 52, ENOTEMPTY: 55, ENAMETOOLONG: 37, ELOOP: 32, EOPNOTSUPP: 138, EPFNOSUPPORT: 139, ECONNRESET: 15, ENOBUFS: 42, EAFNOSUPPORT: 5, EPROTOTYPE: 67, ENOTSOCK: 57, ENOPROTOOPT: 50, ESHUTDOWN: 140, ECONNREFUSED: 14, EADDRINUSE: 3, ECONNABORTED: 13, ENETUNREACH: 40, ENETDOWN: 38, ETIMEDOUT: 73, EHOSTDOWN: 142, EHOSTUNREACH: 23, EINPROGRESS: 26, EALREADY: 7, EDESTADDRREQ: 17, EMSGSIZE: 35, EPROTONOSUPPORT: 66, ESOCKTNOSUPPORT: 137, EADDRNOTAVAIL: 4, ENETRESET: 39, EISCONN: 30, ENOTCONN: 53, ETOOMANYREFS: 141, EUSERS: 136, EDQUOT: 19, ESTALE: 72, ENOTSUP: 138, ENOMEDIUM: 148, EILSEQ: 25, EOVERFLOW: 61, ECANCELED: 11, ENOTRECOVERABLE: 56, EOWNERDEAD: 62, ESTRPIPE: 135 }, An = async (e) => {
    var r = await Ke(e);
    return d(r, `Loading data file "${e}" failed (no arrayBuffer).`), new Uint8Array(r);
  }, Pn = (...e) => i.createDataFile(...e), In = (e) => {
    for (var r = e; ; ) {
      if (!ue[e]) return e;
      e = r + Math.random();
    }
  }, rt = [], Dn = async (e, r) => {
    typeof Browser < "u" && Browser.init();
    for (var t of rt)
      if (t.canHandle(r))
        return d(t.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), t.handle(e, r);
    return e;
  }, tt = async (e, r, t, n, a, o, s, l) => {
    var c = r ? ve.resolve(I.join2(e, r)) : e, _ = In(`cp ${c}`);
    $r(_);
    try {
      var h = t;
      typeof t == "string" && (h = await An(t)), h = await Dn(h, c), l?.(), o || Pn(e, r, h, n, a, s);
    } finally {
      Ur(_);
    }
  }, Rn = (e, r, t, n, a, o, s, l, c, _) => {
    tt(e, r, t, n, a, l, c, _).then(o).catch(s);
  }, i = { root: null, mounts: [], devices: {}, streams: [], nextInode: 1, nameTable: null, currentPath: "/", initialized: !1, ignorePermissions: !0, filesystems: null, syncFSRequests: 0, readFiles: {}, ErrnoError: class extends Error {
    name = "ErrnoError";
    constructor(e) {
      super(ce ? Cn(e) : ""), this.errno = e;
      for (var r in et)
        if (et[r] === e) {
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
    r.follow_mount ??= !0, I.isAbs(e) || (e = i.cwd() + "/" + e);
    e: for (var t = 0; t < 40; t++) {
      for (var n = e.split("/").filter((_) => !!_), a = i.root, o = "/", s = 0; s < n.length; s++) {
        var l = s === n.length - 1;
        if (l && r.parent)
          break;
        if (n[s] !== ".") {
          if (n[s] === "..") {
            if (o = I.dirname(o), i.isRoot(a)) {
              e = o + "/" + n.slice(s + 1).join("/"), t--;
              continue e;
            } else
              a = a.parent;
            continue;
          }
          o = I.join2(o, n[s]);
          try {
            a = i.lookupNode(a, n[s]);
          } catch (_) {
            if (_?.errno === 44 && l && r.noent_okay)
              return { path: o };
            throw _;
          }
          if (i.isMountpoint(a) && (!l || r.follow_mount) && (a = a.mounted.root), i.isLink(a.mode) && (!l || r.follow)) {
            if (!a.node_ops.readlink)
              throw new i.ErrnoError(52);
            var c = a.node_ops.readlink(a);
            I.isAbs(c) || (c = I.dirname(o) + "/" + c), e = c + "/" + n.slice(s + 1).join("/");
            continue e;
          }
        }
      }
      return { path: o, node: a };
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
    for (var n = i.hashName(e.id, r), a = i.nameTable[n]; a; a = a.name_next) {
      var o = a.name;
      if (a.parent.id === e.id && o === r)
        return a;
    }
    return i.lookup(e, r);
  }, createNode(e, r, t, n) {
    d(typeof e == "object");
    var a = new i.FSNode(e, r, t, n);
    return i.hashAddNode(a), a;
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
    } catch (o) {
      return o.errno;
    }
    var a = i.nodePermissions(e, "wx");
    if (a)
      return a;
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
    return d(r >= -1), e = Object.assign(new i.FSStream(), e), r == -1 && (r = i.nextfd()), e.fd = r, i.streams[r] = e, e;
  }, closeStream(e) {
    i.streams[e] = null;
  }, dupStream(e, r = -1) {
    var t = i.createStream(e, r);
    return t.stream_ops?.dup?.(t), t;
  }, doSetAttr(e, r, t) {
    var n = e?.stream_ops.setattr, a = n ? e : r;
    n ??= r.node_ops.setattr, i.checkOpExists(n, 63), n(a, t);
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
    typeof e == "function" && (r = e, e = !1), i.syncFSRequests++, i.syncFSRequests > 1 && k(`warning: ${i.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
    var t = i.getMounts(i.root.mount), n = 0;
    function a(l) {
      return d(i.syncFSRequests > 0), i.syncFSRequests--, r(l);
    }
    function o(l) {
      if (l)
        return o.errored ? void 0 : (o.errored = !0, a(l));
      ++n >= t.length && a(null);
    }
    for (var s of t)
      s.type.syncfs ? s.type.syncfs(s, e, o) : o(null);
  }, mount(e, r, t) {
    if (typeof e == "string")
      throw e;
    var n = t === "/", a = !t, o;
    if (n && i.root)
      throw new i.ErrnoError(10);
    if (!n && !a) {
      var s = i.lookupPath(t, { follow_mount: !1 });
      if (t = s.path, o = s.node, i.isMountpoint(o))
        throw new i.ErrnoError(10);
      if (!i.isDir(o.mode))
        throw new i.ErrnoError(54);
    }
    var l = { type: e, opts: r, mountpoint: t, mounts: [] }, c = e.mount(l);
    return c.mount = l, l.root = c, n ? i.root = c : o && (o.mounted = l, o.mount && o.mount.mounts.push(l)), c;
  }, unmount(e) {
    var r = i.lookupPath(e, { follow_mount: !1 });
    if (!i.isMountpoint(r.node))
      throw new i.ErrnoError(28);
    var t = r.node, n = t.mounted, a = i.getMounts(n);
    for (var [o, s] of Object.entries(i.nameTable))
      for (; s; ) {
        var l = s.name_next;
        a.includes(s.mount) && i.destroyNode(s), s = l;
      }
    t.mounted = null;
    var c = t.mount.mounts.indexOf(n);
    d(c !== -1), t.mount.mounts.splice(c, 1);
  }, lookup(e, r) {
    return e.node_ops.lookup(e, r);
  }, mknod(e, r, t) {
    var n = i.lookupPath(e, { parent: !0 }), a = n.node, o = I.basename(e);
    if (!o)
      throw new i.ErrnoError(28);
    if (o === "." || o === "..")
      throw new i.ErrnoError(20);
    var s = i.mayCreate(a, o);
    if (s)
      throw new i.ErrnoError(s);
    if (!a.node_ops.mknod)
      throw new i.ErrnoError(63);
    return a.node_ops.mknod(a, o, r, t);
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
    for (var a of t)
      if (a) {
        (n || I.isAbs(e)) && (n += "/"), n += a;
        try {
          i.mkdir(n, r);
        } catch (o) {
          if (o.errno != 20) throw o;
        }
      }
  }, mkdev(e, r, t) {
    return typeof t > "u" && (t = r, r = 438), r |= 8192, i.mknod(e, r, t);
  }, symlink(e, r) {
    if (!ve.resolve(e))
      throw new i.ErrnoError(44);
    var t = i.lookupPath(r, { parent: !0 }), n = t.node;
    if (!n)
      throw new i.ErrnoError(44);
    var a = I.basename(r), o = i.mayCreate(n, a);
    if (o)
      throw new i.ErrnoError(o);
    if (!n.node_ops.symlink)
      throw new i.ErrnoError(63);
    return n.node_ops.symlink(n, a, e);
  }, rename(e, r) {
    var t = I.dirname(e), n = I.dirname(r), a = I.basename(e), o = I.basename(r), s, l, c;
    if (s = i.lookupPath(e, { parent: !0 }), l = s.node, s = i.lookupPath(r, { parent: !0 }), c = s.node, !l || !c) throw new i.ErrnoError(44);
    if (l.mount !== c.mount)
      throw new i.ErrnoError(75);
    var _ = i.lookupNode(l, a), h = ve.relative(e, n);
    if (h.charAt(0) !== ".")
      throw new i.ErrnoError(28);
    if (h = ve.relative(r, t), h.charAt(0) !== ".")
      throw new i.ErrnoError(55);
    var w;
    try {
      w = i.lookupNode(c, o);
    } catch {
    }
    if (_ !== w) {
      var p = i.isDir(_.mode), g = i.mayDelete(l, a, p);
      if (g)
        throw new i.ErrnoError(g);
      if (g = w ? i.mayDelete(c, o, p) : i.mayCreate(c, o), g)
        throw new i.ErrnoError(g);
      if (!l.node_ops.rename)
        throw new i.ErrnoError(63);
      if (i.isMountpoint(_) || w && i.isMountpoint(w))
        throw new i.ErrnoError(10);
      if (c !== l && (g = i.nodePermissions(l, "w"), g))
        throw new i.ErrnoError(g);
      i.hashRemoveNode(_);
      try {
        l.node_ops.rename(_, c, o), _.parent = c;
      } catch (F) {
        throw F;
      } finally {
        i.hashAddNode(_);
      }
    }
  }, rmdir(e) {
    var r = i.lookupPath(e, { parent: !0 }), t = r.node, n = I.basename(e), a = i.lookupNode(t, n), o = i.mayDelete(t, n, !0);
    if (o)
      throw new i.ErrnoError(o);
    if (!t.node_ops.rmdir)
      throw new i.ErrnoError(63);
    if (i.isMountpoint(a))
      throw new i.ErrnoError(10);
    t.node_ops.rmdir(t, n), i.destroyNode(a);
  }, readdir(e) {
    var r = i.lookupPath(e, { follow: !0 }), t = r.node, n = i.checkOpExists(t.node_ops.readdir, 54);
    return n(t);
  }, unlink(e) {
    var r = i.lookupPath(e, { parent: !0 }), t = r.node;
    if (!t)
      throw new i.ErrnoError(44);
    var n = I.basename(e), a = i.lookupNode(t, n), o = i.mayDelete(t, n, !1);
    if (o)
      throw new i.ErrnoError(o);
    if (!t.node_ops.unlink)
      throw new i.ErrnoError(63);
    if (i.isMountpoint(a))
      throw new i.ErrnoError(10);
    t.node_ops.unlink(t, n), i.destroyNode(a);
  }, readlink(e) {
    var r = i.lookupPath(e), t = r.node;
    if (!t)
      throw new i.ErrnoError(44);
    if (!t.node_ops.readlink)
      throw new i.ErrnoError(28);
    return t.node_ops.readlink(t);
  }, stat(e, r) {
    var t = i.lookupPath(e, { follow: !r }), n = t.node, a = i.checkOpExists(n.node_ops.getattr, 63);
    return a(n);
  }, fstat(e) {
    var r = i.getStreamChecked(e), t = r.node, n = r.stream_ops.getattr, a = n ? r : t;
    return n ??= t.node_ops.getattr, i.checkOpExists(n, 63), n(a);
  }, lstat(e) {
    return i.stat(e, !0);
  }, doChmod(e, r, t, n) {
    i.doSetAttr(e, r, { mode: t & 4095 | r.mode & -4096, ctime: Date.now(), dontFollow: n });
  }, chmod(e, r, t) {
    var n;
    if (typeof e == "string") {
      var a = i.lookupPath(e, { follow: !t });
      n = a.node;
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
    var a;
    if (typeof e == "string") {
      var o = i.lookupPath(e, { follow: !n });
      a = o.node;
    } else
      a = e;
    i.doChown(null, a, n);
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
    var n = i.lookupPath(e, { follow: !0 }), a = n.node, o = i.checkOpExists(a.node_ops.setattr, 63);
    o(a, { atime: r, mtime: t });
  }, open(e, r, t = 438) {
    if (e === "")
      throw new i.ErrnoError(44);
    r = typeof r == "string" ? Fn(r) : r, r & 64 ? t = t & 4095 | 32768 : t = 0;
    var n, a;
    if (typeof e == "object")
      n = e;
    else {
      a = e.endsWith("/");
      var o = i.lookupPath(e, { follow: !(r & 131072), noent_okay: !0 });
      n = o.node, e = o.path;
    }
    var s = !1;
    if (r & 64)
      if (n) {
        if (r & 128)
          throw new i.ErrnoError(20);
      } else {
        if (a)
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
    var c = i.createStream({ node: n, path: i.getPath(n), flags: r, seekable: !0, position: 0, stream_ops: n.stream_ops, ungotten: [], error: !1 });
    return c.stream_ops.open && c.stream_ops.open(c), s && i.chmod(n, t & 511), f.logReadFiles && !(r & 1) && (e in i.readFiles || (i.readFiles[e] = 1)), c;
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
  }, read(e, r, t, n, a) {
    if (d(t >= 0), n < 0 || a < 0)
      throw new i.ErrnoError(28);
    if (i.isClosed(e))
      throw new i.ErrnoError(8);
    if ((e.flags & 2097155) === 1)
      throw new i.ErrnoError(8);
    if (i.isDir(e.node.mode))
      throw new i.ErrnoError(31);
    if (!e.stream_ops.read)
      throw new i.ErrnoError(28);
    var o = typeof a < "u";
    if (!o)
      a = e.position;
    else if (!e.seekable)
      throw new i.ErrnoError(70);
    var s = e.stream_ops.read(e, r, t, n, a);
    return o || (e.position += s), s;
  }, write(e, r, t, n, a, o) {
    if (d(t >= 0), n < 0 || a < 0)
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
    var s = typeof a < "u";
    if (!s)
      a = e.position;
    else if (!e.seekable)
      throw new i.ErrnoError(70);
    var l = e.stream_ops.write(e, r, t, n, a, o);
    return s || (e.position += l), l;
  }, mmap(e, r, t, n, a) {
    if ((n & 2) !== 0 && (a & 2) === 0 && (e.flags & 2097155) !== 2)
      throw new i.ErrnoError(2);
    if ((e.flags & 2097155) === 1)
      throw new i.ErrnoError(2);
    if (!e.stream_ops.mmap)
      throw new i.ErrnoError(43);
    if (!r)
      throw new i.ErrnoError(28);
    return e.stream_ops.mmap(e, r, t, n, a);
  }, msync(e, r, t, n, a) {
    return d(t >= 0), e.stream_ops.msync ? e.stream_ops.msync(e, r, t, n, a) : 0;
  }, ioctl(e, r, t) {
    if (!e.stream_ops.ioctl)
      throw new i.ErrnoError(59);
    return e.stream_ops.ioctl(e, r, t);
  }, readFile(e, r = {}) {
    r.flags = r.flags || 0, r.encoding = r.encoding || "binary", r.encoding !== "utf8" && r.encoding !== "binary" && P(`Invalid encoding type "${r.encoding}"`);
    var t = i.open(e, r.flags), n = i.stat(e), a = n.size, o = new Uint8Array(a);
    return i.read(t, o, 0, a, 0), r.encoding === "utf8" && (o = _e(o)), i.close(t), o;
  }, writeFile(e, r, t = {}) {
    t.flags = t.flags || 577;
    var n = i.open(e, t.flags, t.mode);
    typeof r == "string" && (r = new Uint8Array(sr(r))), ArrayBuffer.isView(r) ? i.write(n, r, 0, r.byteLength, void 0, t.canOwn) : P("Unsupported data type"), i.close(n);
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
    i.mkdir("/dev"), i.registerDevice(i.makedev(1, 3), { read: () => 0, write: (n, a, o, s, l) => s, llseek: () => 0 }), i.mkdev("/dev/null", i.makedev(1, 3)), re.register(i.makedev(5, 0), re.default_tty_ops), re.register(i.makedev(6, 0), re.default_tty1_ops), i.mkdev("/dev/tty", i.makedev(5, 0)), i.mkdev("/dev/tty1", i.makedev(6, 0));
    var e = new Uint8Array(1024), r = 0, t = () => (r === 0 && (ar(e), r = e.byteLength), e[--r]);
    i.createDevice("/dev", "random", t), i.createDevice("/dev", "urandom", t), i.mkdir("/dev/shm"), i.mkdir("/dev/shm/tmp");
  }, createSpecialDirectories() {
    i.mkdir("/proc");
    var e = i.mkdir("/proc/self");
    i.mkdir("/proc/self/fd"), i.mount({ mount() {
      var r = i.createNode(e, "fd", 16895, 73);
      return r.stream_ops = { llseek: b.stream_ops.llseek }, r.node_ops = { lookup(t, n) {
        var a = +n, o = i.getStreamChecked(a), s = { parent: null, mount: { mountpoint: "fake" }, node_ops: { readlink: () => o.path }, id: a + 1 };
        return s.parent = s, s;
      }, readdir() {
        return Array.from(i.streams.entries()).filter(([t, n]) => n).map(([t, n]) => t.toString());
      } }, r;
    } }, {}, "/proc/self/fd");
  }, createStandardStreams(e, r, t) {
    e ? i.createDevice("/dev", "stdin", e) : i.symlink("/dev/tty", "/dev/stdin"), r ? i.createDevice("/dev", "stdout", null, r) : i.symlink("/dev/tty", "/dev/stdout"), t ? i.createDevice("/dev", "stderr", null, t) : i.symlink("/dev/tty1", "/dev/stderr");
    var n = i.open("/dev/stdin", 0), a = i.open("/dev/stdout", 1), o = i.open("/dev/stderr", 1);
    d(n.fd === 0, `invalid handle for stdin (${n.fd})`), d(a.fd === 1, `invalid handle for stdout (${a.fd})`), d(o.fd === 2, `invalid handle for stderr (${o.fd})`);
  }, staticInit() {
    i.nameTable = new Array(4096), i.mount(b, {}, "/"), i.createDefaultDirectories(), i.createDefaultDevices(), i.createSpecialDirectories(), i.filesystems = { MEMFS: b };
  }, init(e, r, t) {
    d(!i.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), i.initialized = !0, e ??= f.stdin, r ??= f.stdout, t ??= f.stderr, i.createStandardStreams(e, r, t);
  }, quit() {
    i.initialized = !1, pr(0);
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
      n.parentExists = !0, n.parentPath = t.path, n.parentObject = t.node, n.name = I.basename(e), t = i.lookupPath(e, { follow: !r }), n.exists = !0, n.path = t.path, n.object = t.node, n.name = t.node.name, n.isRoot = t.path === "/";
    } catch (a) {
      n.error = a.errno;
    }
    return n;
  }, createPath(e, r, t, n) {
    e = typeof e == "string" ? e : i.getPath(e);
    for (var a = r.split("/").reverse(); a.length; ) {
      var o = a.pop();
      if (o) {
        var s = I.join2(e, o);
        try {
          i.mkdir(s);
        } catch (l) {
          if (l.errno != 20) throw l;
        }
        e = s;
      }
    }
    return s;
  }, createFile(e, r, t, n, a) {
    var o = I.join2(typeof e == "string" ? e : i.getPath(e), r), s = lr(n, a);
    return i.create(o, s);
  }, createDataFile(e, r, t, n, a, o) {
    var s = r;
    e && (e = typeof e == "string" ? e : i.getPath(e), s = r ? I.join2(e, r) : e);
    var l = lr(n, a), c = i.create(s, l);
    if (t) {
      if (typeof t == "string") {
        for (var _ = new Array(t.length), h = 0, w = t.length; h < w; ++h) _[h] = t.charCodeAt(h);
        t = _;
      }
      i.chmod(c, l | 146);
      var p = i.open(c, 577);
      i.write(p, t, 0, t.length, 0, o), i.close(p), i.chmod(c, l);
    }
  }, createDevice(e, r, t, n) {
    var a = I.join2(typeof e == "string" ? e : i.getPath(e), r), o = lr(!!t, !!n);
    i.createDevice.major ??= 64;
    var s = i.makedev(i.createDevice.major++, 0);
    return i.registerDevice(s, { open(l) {
      l.seekable = !1;
    }, close(l) {
      n?.buffer?.length && n(10);
    }, read(l, c, _, h, w) {
      for (var p = 0, g = 0; g < h; g++) {
        var F;
        try {
          F = t();
        } catch {
          throw new i.ErrnoError(29);
        }
        if (F === void 0 && p === 0)
          throw new i.ErrnoError(6);
        if (F == null) break;
        p++, c[_ + g] = F;
      }
      return p && (l.node.atime = Date.now()), p;
    }, write(l, c, _, h, w) {
      for (var p = 0; p < h; p++)
        try {
          n(c[_ + p]);
        } catch {
          throw new i.ErrnoError(29);
        }
      return h && (l.node.mtime = l.node.ctime = Date.now()), p;
    } }), i.mkdev(a, o, s);
  }, forceLoadFile(e) {
    if (e.isDevice || e.isFolder || e.link || e.contents) return !0;
    if (globalThis.XMLHttpRequest)
      P("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    else
      try {
        e.contents = Ie(e.url);
      } catch {
        throw new i.ErrnoError(29);
      }
  }, createLazyFile(e, r, t, n, a) {
    class o {
      lengthKnown = !1;
      chunks = [];
      get(p) {
        if (!(p > this.length - 1 || p < 0)) {
          var g = p % this.chunkSize, F = p / this.chunkSize | 0;
          return this.getter(F)[g];
        }
      }
      setDataGetter(p) {
        this.getter = p;
      }
      cacheLength() {
        var p = new XMLHttpRequest();
        p.open("HEAD", t, !1), p.send(null), p.status >= 200 && p.status < 300 || p.status === 304 || P("Couldn't load " + t + ". Status: " + p.status);
        var g = Number(p.getResponseHeader("Content-length")), F, R = (F = p.getResponseHeader("Accept-Ranges")) && F === "bytes", A = (F = p.getResponseHeader("Content-Encoding")) && F === "gzip", N = 1024 * 1024;
        R || (N = g);
        var j = (G, we) => {
          G > we && P("invalid range (" + G + ", " + we + ") or no bytes requested!"), we > g - 1 && P("only " + g + " bytes available! programmer error!");
          var x = new XMLHttpRequest();
          return x.open("GET", t, !1), g !== N && x.setRequestHeader("Range", "bytes=" + G + "-" + we), x.responseType = "arraybuffer", x.overrideMimeType && x.overrideMimeType("text/plain; charset=x-user-defined"), x.send(null), x.status >= 200 && x.status < 300 || x.status === 304 || P("Couldn't load " + t + ". Status: " + x.status), x.response !== void 0 ? new Uint8Array(x.response || []) : sr(x.responseText || "");
        }, Ae = this;
        Ae.setDataGetter((G) => {
          var we = G * N, x = (G + 1) * N - 1;
          return x = Math.min(x, g - 1), typeof Ae.chunks[G] > "u" && (Ae.chunks[G] = j(we, x)), typeof Ae.chunks[G] > "u" && P("doXHR failed!"), Ae.chunks[G];
        }), (A || !g) && (N = g = 1, g = this.getter(0).length, N = g, Q("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = g, this._chunkSize = N, this.lengthKnown = !0;
      }
      get length() {
        return this.lengthKnown || this.cacheLength(), this._length;
      }
      get chunkSize() {
        return this.lengthKnown || this.cacheLength(), this._chunkSize;
      }
    }
    if (globalThis.XMLHttpRequest) {
      Z || P("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
      var s = new o(), l = { isDevice: !1, contents: s };
    } else
      var l = { isDevice: !1, url: t };
    var c = i.createFile(e, r, l, n, a);
    l.contents ? c.contents = l.contents : l.url && (c.contents = null, c.url = l.url), Object.defineProperties(c, { usedBytes: { get: function() {
      return this.contents.length;
    } } });
    var _ = {};
    for (const [w, p] of Object.entries(c.stream_ops))
      _[w] = (...g) => (i.forceLoadFile(c), p(...g));
    function h(w, p, g, F, R) {
      var A = w.node.contents;
      if (R >= A.length) return 0;
      var N = Math.min(A.length - R, F);
      if (d(N >= 0), A.slice)
        for (var j = 0; j < N; j++)
          p[g + j] = A[R + j];
      else
        for (var j = 0; j < N; j++)
          p[g + j] = A.get(R + j);
      return N;
    }
    return _.read = (w, p, g, F, R) => (i.forceLoadFile(c), h(w, p, g, F, R)), _.mmap = (w, p, g, F, R) => {
      i.forceLoadFile(c);
      var A = Qr(p);
      if (!A)
        throw new i.ErrnoError(48);
      return h(w, (u(), L), A, p, g), { ptr: A, allocated: !0 };
    }, c.stream_ops = _, c;
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
  } }, D = { DEFAULT_POLLMASK: 5, calculateAt(e, r, t) {
    if (I.isAbs(r))
      return r;
    var n;
    if (e === -100)
      n = i.cwd();
    else {
      var a = D.getStreamFromFD(e);
      n = a.path;
    }
    if (r.length == 0) {
      if (!t)
        throw new i.ErrnoError(44);
      return n;
    }
    return n + "/" + r;
  }, writeStat(e, r) {
    (u(), y)[e >> 2] = r.dev, (u(), y)[e + 4 >> 2] = r.mode, (u(), y)[e + 8 >> 2] = r.nlink, (u(), y)[e + 12 >> 2] = r.uid, (u(), y)[e + 16 >> 2] = r.gid, (u(), y)[e + 20 >> 2] = r.rdev, (u(), O)[e + 24 >> 3] = BigInt(r.size), (u(), M)[e + 32 >> 2] = 4096, (u(), M)[e + 36 >> 2] = r.blocks;
    var t = r.atime.getTime(), n = r.mtime.getTime(), a = r.ctime.getTime();
    return (u(), O)[e + 40 >> 3] = BigInt(Math.floor(t / 1e3)), (u(), y)[e + 48 >> 2] = t % 1e3 * 1e3 * 1e3, (u(), O)[e + 56 >> 3] = BigInt(Math.floor(n / 1e3)), (u(), y)[e + 64 >> 2] = n % 1e3 * 1e3 * 1e3, (u(), O)[e + 72 >> 3] = BigInt(Math.floor(a / 1e3)), (u(), y)[e + 80 >> 2] = a % 1e3 * 1e3 * 1e3, (u(), O)[e + 88 >> 3] = BigInt(r.ino), 0;
  }, writeStatFs(e, r) {
    (u(), y)[e + 4 >> 2] = r.bsize, (u(), y)[e + 60 >> 2] = r.bsize, (u(), O)[e + 8 >> 3] = BigInt(r.blocks), (u(), O)[e + 16 >> 3] = BigInt(r.bfree), (u(), O)[e + 24 >> 3] = BigInt(r.bavail), (u(), O)[e + 32 >> 3] = BigInt(r.files), (u(), O)[e + 40 >> 3] = BigInt(r.ffree), (u(), y)[e + 48 >> 2] = r.fsid, (u(), y)[e + 64 >> 2] = r.flags, (u(), y)[e + 56 >> 2] = r.namelen;
  }, doMsync(e, r, t, n, a) {
    if (!i.isFile(r.node.mode))
      throw new i.ErrnoError(43);
    if (n & 2)
      return 0;
    var o = (u(), U).slice(e, e + t);
    i.msync(r, o, a, t, n);
  }, getStreamFromFD(e) {
    var r = i.getStreamChecked(e);
    return r;
  }, varargs: void 0, getStr(e) {
    var r = K(e);
    return r;
  } };
  function nt(e, r, t) {
    if (S) return W(3, 0, 1, e, r, t);
    D.varargs = t;
    try {
      var n = D.getStreamFromFD(e);
      switch (r) {
        case 0: {
          var a = Ne();
          if (a < 0)
            return -28;
          for (; i.streams[a]; )
            a++;
          var o;
          return o = i.dupStream(n, a), o.fd;
        }
        case 1:
        case 2:
          return 0;
        case 3:
          return n.flags;
        case 4: {
          var a = Ne();
          return n.flags |= a, 0;
        }
        case 12: {
          var a = me(), s = 0;
          return (u(), ne)[a + s >> 1] = 2, 0;
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
  function it(e, r) {
    if (S) return W(4, 0, 1, e, r);
    try {
      return D.writeStat(r, i.fstat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function at(e, r, t) {
    if (S) return W(5, 0, 1, e, r, t);
    D.varargs = t;
    try {
      var n = D.getStreamFromFD(e);
      switch (r) {
        case 21509:
          return n.tty ? 0 : -59;
        case 21505: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tcgets) {
            var a = n.tty.ops.ioctl_tcgets(n), o = me();
            (u(), M)[o >> 2] = a.c_iflag || 0, (u(), M)[o + 4 >> 2] = a.c_oflag || 0, (u(), M)[o + 8 >> 2] = a.c_cflag || 0, (u(), M)[o + 12 >> 2] = a.c_lflag || 0;
            for (var s = 0; s < 32; s++)
              (u(), L)[o + s + 17] = a.c_cc[s] || 0;
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
            for (var o = me(), l = (u(), M)[o >> 2], c = (u(), M)[o + 4 >> 2], _ = (u(), M)[o + 8 >> 2], h = (u(), M)[o + 12 >> 2], w = [], s = 0; s < 32; s++)
              w.push((u(), L)[o + s + 17]);
            return n.tty.ops.ioctl_tcsets(n.tty, r, { c_iflag: l, c_oflag: c, c_cflag: _, c_lflag: h, c_cc: w });
          }
          return 0;
        }
        case 21519: {
          if (!n.tty) return -59;
          var o = me();
          return (u(), M)[o >> 2] = 0, 0;
        }
        case 21520:
          return n.tty ? -28 : -59;
        case 21537:
        case 21531: {
          var o = me();
          return i.ioctl(n, r, o);
        }
        case 21523: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tiocgwinsz) {
            var p = n.tty.ops.ioctl_tiocgwinsz(n.tty), o = me();
            (u(), ne)[o >> 1] = p[0], (u(), ne)[o + 2 >> 1] = p[1];
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
    } catch (g) {
      if (typeof i > "u" || g.name !== "ErrnoError") throw g;
      return -g.errno;
    }
  }
  function ot(e, r) {
    if (S) return W(6, 0, 1, e, r);
    try {
      return e = D.getStr(e), D.writeStat(r, i.lstat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function st(e, r, t, n) {
    if (S) return W(7, 0, 1, e, r, t, n);
    try {
      r = D.getStr(r);
      var a = n & 256, o = n & 4096;
      return n = n & -6401, d(!n, `unknown flags in __syscall_newfstatat: ${n}`), r = D.calculateAt(e, r, o), D.writeStat(t, a ? i.lstat(r) : i.stat(r));
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return -s.errno;
    }
  }
  function lt(e, r, t, n) {
    if (S) return W(8, 0, 1, e, r, t, n);
    D.varargs = n;
    try {
      r = D.getStr(r), r = D.calculateAt(e, r);
      var a = n ? Ne() : 0;
      return i.open(r, t, a).fd;
    } catch (o) {
      if (typeof i > "u" || o.name !== "ErrnoError") throw o;
      return -o.errno;
    }
  }
  function dt(e, r) {
    if (S) return W(9, 0, 1, e, r);
    try {
      return e = D.getStr(e), D.writeStat(r, i.stat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  var Mn = () => P("native code called abort()"), $ = (e) => {
    for (var r = ""; ; ) {
      var t = (u(), U)[e++];
      if (!t) return r;
      r += String.fromCharCode(t);
    }
  }, pe = {}, he = {}, Oe = {}, Nn = class extends Error {
    constructor(r) {
      super(r), this.name = "BindingError";
    }
  }, B = (e) => {
    throw new Nn(e);
  };
  function On(e, r, t = {}) {
    var n = r.name;
    if (e || B(`type "${n}" must have a positive integer typeid pointer`), he.hasOwnProperty(e)) {
      if (t.ignoreDuplicateRegistrations)
        return;
      B(`Cannot register type '${n}' twice`);
    }
    if (he[e] = r, delete Oe[e], pe.hasOwnProperty(e)) {
      var a = pe[e];
      delete pe[e], a.forEach((o) => o());
    }
  }
  function H(e, r, t = {}) {
    return On(e, r, t);
  }
  var ct = (e, r, t) => {
    switch (r) {
      case 1:
        return t ? (n) => (u(), L)[n] : (n) => (u(), U)[n];
      case 2:
        return t ? (n) => (u(), ne)[n >> 1] : (n) => (u(), be)[n >> 1];
      case 4:
        return t ? (n) => (u(), M)[n >> 2] : (n) => (u(), y)[n >> 2];
      case 8:
        return t ? (n) => (u(), O)[n >> 3] : (n) => (u(), Ir)[n >> 3];
      default:
        throw new TypeError(`invalid integer width (${r}): ${e}`);
    }
  }, We = (e) => {
    if (e === null)
      return "null";
    var r = typeof e;
    return r === "object" || r === "array" || r === "function" ? e.toString() : "" + e;
  }, ut = (e, r, t, n) => {
    if (r < t || r > n)
      throw new TypeError(`Passing a number "${We(r)}" from JS side to C/C++ side to an argument of type "${e}", which is outside the valid range [${t}, ${n}]!`);
  }, Wn = (e, r, t, n, a) => {
    r = $(r);
    const o = n === 0n;
    let s = (l) => l;
    if (o) {
      const l = t * 8;
      s = (c) => BigInt.asUintN(l, c), a = s(a);
    }
    H(e, { name: r, fromWireType: s, toWireType: (l, c) => {
      if (typeof c == "number")
        c = BigInt(c);
      else if (typeof c != "bigint")
        throw new TypeError(`Cannot convert "${We(c)}" to ${this.name}`);
      return ut(r, c, n, a), c;
    }, readValueFromPointer: ct(r, t, !o), destructorFunction: null });
  }, Ln = (e, r, t, n) => {
    r = $(r), H(e, { name: r, fromWireType: function(a) {
      return !!a;
    }, toWireType: function(a, o) {
      return o ? t : n;
    }, readValueFromPointer: function(a) {
      return this.fromWireType((u(), U)[a]);
    }, destructorFunction: null });
  }, ft = [], X = [0, 1, , 1, null, 1, !0, 1, !1, 1], xn = (e) => {
    e > 9 && --X[e + 1] === 0 && (d(X[e] !== void 0, "Decref for unallocated handle."), X[e] = void 0, ft.push(e));
  }, _t = { toValue: (e) => (e || B(`Cannot use deleted val. handle = ${e}`), d(e === 2 || X[e] !== void 0 && e % 2 === 0, `invalid handle: ${e}`), X[e]), toHandle: (e) => {
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
        const r = ft.pop() || X.length;
        return X[r] = e, X[r + 1] = 1, r;
      }
    }
  } };
  function dr(e) {
    return this.fromWireType((u(), y)[e >> 2]);
  }
  var jn = { name: "emscripten::val", fromWireType: (e) => {
    var r = _t.toValue(e);
    return xn(e), r;
  }, toWireType: (e, r) => _t.toHandle(r), readValueFromPointer: dr, destructorFunction: null }, Un = (e) => H(e, jn), $n = (e, r) => {
    switch (r) {
      case 4:
        return function(t) {
          return this.fromWireType((u(), Pr)[t >> 2]);
        };
      case 8:
        return function(t) {
          return this.fromWireType((u(), ke)[t >> 3]);
        };
      default:
        throw new TypeError(`invalid float width (${r}): ${e}`);
    }
  }, Bn = (e, r, t) => {
    r = $(r), H(e, { name: r, fromWireType: (n) => n, toWireType: (n, a) => {
      if (typeof a != "number" && typeof a != "boolean")
        throw new TypeError(`Cannot convert ${We(a)} to ${this.name}`);
      return a;
    }, readValueFromPointer: $n(r, t), destructorFunction: null });
  }, mt = (e, r) => Object.defineProperty(r, "name", { value: e }), zn = (e) => {
    for (; e.length; ) {
      var r = e.pop(), t = e.pop();
      t(r);
    }
  };
  function vt(e) {
    for (var r = 1; r < e.length; ++r)
      if (e[r] !== null && e[r].destructorFunction === void 0)
        return !0;
    return !1;
  }
  function Hn(e, r, t, n, a) {
    if (e < r || e > t) {
      var o = r == t ? r : `${r} to ${t}`;
      a(`function ${n} called with ${e} arguments, expected ${o}`);
    }
  }
  function Vn(e, r, t, n) {
    for (var a = vt(e), o = e.length - 2, s = [], l = ["fn"], c = 0; c < o; ++c)
      s.push(`arg${c}`), l.push(`arg${c}Wired`);
    s = s.join(","), l = l.join(",");
    var _ = `return function (${s}) {
`;
    _ += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, a && (_ += `var destructors = [];
`);
    for (var h = a ? "destructors" : "null", w = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "fromRetWire", "toClassParamWire"], c = 0; c < o; ++c) {
      var p = `toArg${c}Wire`;
      _ += `var arg${c}Wired = ${p}(${h}, arg${c});
`, w.push(p);
    }
    _ += (t || n ? "var rv = " : "") + `invoker(${l});
`;
    var g = t ? "rv" : "";
    if (w.push("Asyncify"), _ += `function onDone(${g}) {
`, a)
      _ += `runDestructors(destructors);
`;
    else
      for (var c = 2; c < e.length; ++c) {
        var F = c === 1 ? "thisWired" : "arg" + (c - 2) + "Wired";
        e[c].destructorFunction !== null && (_ += `${F}_dtor(${F});
`, w.push(`${F}_dtor`));
      }
    return t && (_ += `var ret = fromRetWire(rv);
return ret;
`), _ += `}
`, _ += `return Asyncify.currData ? Asyncify.whenDone().then(onDone) : onDone(${g});
`, _ += `}
`, w.push("checkArgCount", "minArgs", "maxArgs"), _ = `if (arguments.length !== ${w.length}){ throw new Error(humanName + "Expected ${w.length} closure arguments " + arguments.length + " given."); }
${_}`, new Function(w, _);
  }
  var Le = (e) => {
    try {
      return e();
    } catch (r) {
      P(r);
    }
  }, pt = (e) => {
    if (e instanceof Nr || e == "unwind")
      return de;
    Se(), e instanceof WebAssembly.RuntimeError && wr() <= 0 && k("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), br(1, e);
  }, Gn = () => {
    if (!Me())
      try {
        if (S) {
          ye() && gr(de);
          return;
        }
        nr(de);
      } catch (e) {
        pt(e);
      }
  }, cr = (e) => {
    if (q) {
      k("user callback triggered after runtime exited or application aborted.  Ignoring.");
      return;
    }
    try {
      e(), Gn();
    } catch (r) {
      pt(r);
    }
  }, ur = () => {
    fe += 1;
  }, ht = () => {
    d(fe > 0), fe -= 1;
  }, m = { instrumentWasmImports(e) {
    var r = /^(invoke_.*|__asyncjs__.*)$/;
    for (let [t, n] of Object.entries(e))
      if (typeof n == "function") {
        let a = n.isAsync || r.test(t);
        e[t] = (...o) => {
          var s = m.state;
          try {
            return n(...o);
          } finally {
            var l = s === m.State.Normal && m.state === m.State.Disabled, c = t.startsWith("invoke_") && !0;
            m.state !== s && !a && !l && !c && P(`import ${t} was not in ASYNCIFY_IMPORTS, but changed the state`);
          }
        };
      }
  }, instrumentFunction(e) {
    var r = (...t) => {
      m.exportCallStack.push(e);
      try {
        return e(...t);
      } finally {
        if (!q) {
          var n = m.exportCallStack.pop();
          d(n === e), m.maybeStopUnwind();
        }
      }
    };
    return m.funcWrappers.set(e, r), r = mt(`__asyncify_wrapper_${e.name}`, r), r;
  }, instrumentWasmExports(e) {
    var r = {};
    for (let [n, a] of Object.entries(e))
      if (typeof a == "function") {
        var t = m.instrumentFunction(a);
        r[n] = t;
      } else
        r[n] = a;
    return r;
  }, State: { Normal: 0, Unwinding: 1, Rewinding: 2, Disabled: 3 }, state: 0, StackSize: 4096, currData: null, handleSleepReturnValue: 0, exportCallStack: [], callstackFuncToId: /* @__PURE__ */ new Map(), callStackIdToFunc: /* @__PURE__ */ new Map(), funcWrappers: /* @__PURE__ */ new Map(), callStackId: 0, asyncPromiseHandlers: null, sleepCallbacks: [], getCallStackId(e) {
    if (d(e), !m.callstackFuncToId.has(e)) {
      var r = m.callStackId++;
      m.callstackFuncToId.set(e, r), m.callStackIdToFunc.set(r, e);
    }
    return m.callstackFuncToId.get(e);
  }, maybeStopUnwind() {
    m.currData && m.state === m.State.Unwinding && m.exportCallStack.length === 0 && (m.state = m.State.Normal, ur(), Le(Kt), typeof Fibers < "u" && Fibers.trampoline());
  }, whenDone() {
    return d(m.currData, "Tried to wait for an async operation when none is in progress."), d(!m.asyncPromiseHandlers, "Cannot have multiple async operations in flight at once"), new Promise((e, r) => {
      m.asyncPromiseHandlers = { resolve: e, reject: r };
    });
  }, allocateData() {
    var e = Ce(12 + m.StackSize);
    return m.setDataHeader(e, e + 12, m.StackSize), m.setDataRewindFunc(e), e;
  }, setDataHeader(e, r, t) {
    (u(), y)[e >> 2] = r, (u(), y)[e + 4 >> 2] = r + t;
  }, setDataRewindFunc(e) {
    var r = m.exportCallStack[0];
    d(r, "exportCallStack is empty");
    var t = m.getCallStackId(r);
    (u(), M)[e + 8 >> 2] = t;
  }, getDataRewindFunc(e) {
    var r = (u(), M)[e + 8 >> 2], t = m.callStackIdToFunc.get(r);
    return d(t, `id ${r} not found in callStackIdToFunc`), t;
  }, doRewind(e) {
    var r = m.getDataRewindFunc(e), t = m.funcWrappers.get(r);
    return d(r), d(t), ht(), t();
  }, handleSleep(e) {
    if (d(m.state !== m.State.Disabled, "Asyncify cannot be done during or after the runtime exits"), !q) {
      if (m.state === m.State.Normal) {
        var r = !1, t = !1;
        e((n = 0) => {
          if (d(!n || typeof n == "number" || typeof n == "boolean"), !q && (m.handleSleepReturnValue = n, r = !0, !!t)) {
            d(!m.exportCallStack.length, "Waking up (starting to rewind) must be done from JS, without compiled code on the stack."), m.state = m.State.Rewinding, Le(() => Xt(m.currData)), typeof MainLoop < "u" && MainLoop.func && MainLoop.resume();
            var a, o = !1;
            try {
              a = m.doRewind(m.currData);
            } catch (c) {
              a = c, o = !0;
            }
            var s = !1;
            if (!m.currData) {
              var l = m.asyncPromiseHandlers;
              l && (m.asyncPromiseHandlers = null, (o ? l.reject : l.resolve)(a), s = !0);
            }
            if (o && !s)
              throw a;
          }
        }), t = !0, r || (m.state = m.State.Unwinding, m.currData = m.allocateData(), typeof MainLoop < "u" && MainLoop.func && MainLoop.pause(), Le(() => qt(m.currData)));
      } else m.state === m.State.Rewinding ? (m.state = m.State.Normal, Le(Yt), V(m.currData), m.currData = null, m.sleepCallbacks.forEach(cr)) : P(`invalid state: ${m.state}`);
      return m.handleSleepReturnValue;
    }
  }, handleAsync: (e) => m.handleSleep((r) => {
    e().then(r);
  }) };
  function qn(e) {
    for (var r = e.length - 2, t = e.length - 1; t >= 2 && e[t].optional; --t)
      r--;
    return r;
  }
  function Kn(e, r, t, n, a, o) {
    var s = r.length;
    s < 2 && B("argTypes array size mismatch! Must at least get return value and 'this' types!"), d(!o, "Async bindings are only supported with JSPI.");
    for (var l = r[1] !== null && t !== null, c = vt(r), _ = !r[0].isVoid, h = s - 2, w = qn(r), p = r[0], g = r[1], F = [e, B, n, a, zn, p.fromWireType.bind(p), g?.toWireType.bind(g)], R = 2; R < s; ++R) {
      var A = r[R];
      F.push(A.toWireType.bind(A));
    }
    if (F.push(m), !c)
      for (var R = 2; R < r.length; ++R)
        r[R].destructorFunction !== null && F.push(r[R].destructorFunction);
    F.push(Hn, w, h);
    var j = Vn(r, l, _, o)(...F);
    return mt(e, j);
  }
  var Xn = (e, r, t) => {
    if (e[r].overloadTable === void 0) {
      var n = e[r];
      e[r] = function(...a) {
        return e[r].overloadTable.hasOwnProperty(a.length) || B(`Function '${t}' called with an invalid number of arguments (${a.length}) - expects one of (${e[r].overloadTable})!`), e[r].overloadTable[a.length].apply(this, a);
      }, e[r].overloadTable = [], e[r].overloadTable[n.argCount] = n;
    }
  }, Yn = (e, r, t) => {
    f.hasOwnProperty(e) ? ((t === void 0 || f[e].overloadTable !== void 0 && f[e].overloadTable[t] !== void 0) && B(`Cannot register public name '${e}' twice`), Xn(f, e, e), f[e].overloadTable.hasOwnProperty(t) && B(`Cannot register multiple overloads of a function with the same number of arguments (${t})!`), f[e].overloadTable[t] = r) : (f[e] = r, f[e].argCount = t);
  }, Jn = (e, r) => {
    for (var t = [], n = 0; n < e; n++)
      t.push((u(), y)[r + n * 4 >> 2]);
    return t;
  }, Zn = class extends Error {
    constructor(r) {
      super(r), this.name = "InternalError";
    }
  }, yt = (e) => {
    throw new Zn(e);
  }, Qn = (e, r, t) => {
    f.hasOwnProperty(e) || yt("Replacing nonexistent public symbol"), f[e].overloadTable !== void 0 && t !== void 0 ? f[e].overloadTable[t] = r : (f[e] = r, f[e].argCount = t);
  }, ei = (e, r, t = !1) => (...n) => pn(e, r, n, t), ri = (e, r, t = !1) => {
    d(!t, "Async bindings are only supported with JSPI."), e = $(e);
    function n() {
      return ei(e, r);
    }
    var a = n();
    return typeof a != "function" && B(`unknown function pointer with signature ${e}: ${r}`), a;
  };
  class ti extends Error {
  }
  var ni = (e) => {
    var r = Nt(e), t = $(r);
    return V(r), t;
  }, ii = (e, r) => {
    var t = [], n = {};
    function a(o) {
      if (!n[o] && !he[o]) {
        if (Oe[o]) {
          Oe[o].forEach(a);
          return;
        }
        t.push(o), n[o] = !0;
      }
    }
    throw r.forEach(a), new ti(`${e}: ` + t.map(ni).join([", "]));
  }, ai = (e, r, t) => {
    e.forEach((l) => Oe[l] = r);
    function n(l) {
      var c = t(l);
      c.length !== e.length && yt("Mismatched type converter count");
      for (var _ = 0; _ < e.length; ++_)
        H(e[_], c[_]);
    }
    var a = new Array(r.length), o = [], s = 0;
    for (let [l, c] of r.entries())
      he.hasOwnProperty(c) ? a[l] = he[c] : (o.push(c), pe.hasOwnProperty(c) || (pe[c] = []), pe[c].push(() => {
        a[l] = he[c], ++s, s === o.length && n(a);
      }));
    o.length === 0 && n(a);
  }, oi = (e) => {
    e = e.trim();
    const r = e.indexOf("(");
    return r === -1 ? e : (d(e.endsWith(")"), "Parentheses for argument names should match."), e.slice(0, r));
  }, si = (e, r, t, n, a, o, s, l) => {
    var c = Jn(r, t);
    e = $(e), e = oi(e), a = ri(n, a, s), Yn(e, function() {
      ii(`Cannot call ${e} due to unbound types`, c);
    }, r - 1), ai([], c, (_) => {
      var h = [_[0], null].concat(_.slice(1));
      return Qn(e, Kn(e, h, null, a, o, s), r - 1), [];
    });
  }, li = (e, r, t, n, a) => {
    r = $(r);
    const o = n === 0;
    let s = (c) => c;
    if (o) {
      var l = 32 - 8 * t;
      s = (c) => c << l >>> l, a = s(a);
    }
    H(e, { name: r, fromWireType: s, toWireType: (c, _) => {
      if (typeof _ != "number" && typeof _ != "boolean")
        throw new TypeError(`Cannot convert "${We(_)}" to ${r}`);
      return ut(r, _, n, a), _;
    }, readValueFromPointer: ct(r, t, n !== 0), destructorFunction: null });
  }, di = (e, r, t) => {
    var n = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array], a = n[r];
    function o(s) {
      var l = (u(), y)[s >> 2], c = (u(), y)[s + 4 >> 2];
      return new a((u(), L).buffer, c, l);
    }
    t = $(t), H(e, { name: t, fromWireType: o, readValueFromPointer: o }, { ignoreDuplicateRegistrations: !0 });
  }, Y = (e, r, t) => (d(typeof t == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), Jr(e, (u(), U), r, t)), ci = (e, r) => {
    r = $(r), H(e, { name: r, fromWireType(t) {
      var n = (u(), y)[t >> 2], a = t + 4, o;
      return o = K(a, n, !0), V(t), o;
    }, toWireType(t, n) {
      n instanceof ArrayBuffer && (n = new Uint8Array(n));
      var a, o = typeof n == "string";
      o || ArrayBuffer.isView(n) && n.BYTES_PER_ELEMENT == 1 || B("Cannot pass non-string to std::string"), o ? a = se(n) : a = n.length;
      var s = Ce(4 + a + 1), l = s + 4;
      return (u(), y)[s >> 2] = a, o ? Y(n, l, a + 1) : (u(), U).set(n, l), t !== null && t.push(V, s), s;
    }, readValueFromPointer: dr, destructorFunction(t) {
      V(t);
    } });
  }, gt = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, ui = (e, r, t) => {
    d(e % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
    var n = e >> 1, a = Kr((u(), be), n, r / 2, t);
    if (a - n > 16 && gt) return gt.decode((u(), be).slice(n, a));
    for (var o = "", s = n; s < a; ++s) {
      var l = (u(), be)[s];
      o += String.fromCharCode(l);
    }
    return o;
  }, fi = (e, r, t) => {
    if (d(r % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), d(typeof t == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 2) return 0;
    t -= 2;
    for (var n = r, a = t < e.length * 2 ? t / 2 : e.length, o = 0; o < a; ++o) {
      var s = e.charCodeAt(o);
      (u(), ne)[r >> 1] = s, r += 2;
    }
    return (u(), ne)[r >> 1] = 0, r - n;
  }, _i = (e) => e.length * 2, mi = (e, r, t) => {
    d(e % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
    for (var n = "", a = e >> 2, o = 0; !(o >= r / 4); o++) {
      var s = (u(), y)[a + o];
      if (!s && !t) break;
      n += String.fromCodePoint(s);
    }
    return n;
  }, vi = (e, r, t) => {
    if (d(r % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), d(typeof t == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 4) return 0;
    for (var n = r, a = n + t - 4, o = 0; o < e.length; ++o) {
      var s = e.codePointAt(o);
      if (s > 65535 && o++, (u(), M)[r >> 2] = s, r += 4, r + 4 > a) break;
    }
    return (u(), M)[r >> 2] = 0, r - n;
  }, pi = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.codePointAt(t);
      n > 65535 && t++, r += 4;
    }
    return r;
  }, hi = (e, r, t) => {
    t = $(t);
    var n, a, o;
    r === 2 ? (n = ui, a = fi, o = _i) : (d(r === 4, "only 2-byte and 4-byte strings are currently supported"), n = mi, a = vi, o = pi), H(e, { name: t, fromWireType: (s) => {
      var l = (u(), y)[s >> 2], c = n(s + 4, l * r, !0);
      return V(s), c;
    }, toWireType: (s, l) => {
      typeof l != "string" && B(`Cannot pass non-string to C++ string type ${t}`);
      var c = o(l), _ = Ce(4 + c + r);
      return (u(), y)[_ >> 2] = c / r, a(l, _ + 4, c + r), s !== null && s.push(V, _), _;
    }, readValueFromPointer: dr, destructorFunction(s) {
      V(s);
    } });
  }, yi = (e, r) => {
    r = $(r), H(e, { isVoid: !0, name: r, fromWireType: () => {
    }, toWireType: (t, n) => {
    } });
  }, gi = (e) => {
    hr(e, !Z, 1, !Pe, 65536, !1), T.threadInitTLS();
  }, fr = (e) => {
    if (Atomics.waitAsync) {
      var r = Atomics.waitAsync((u(), M), e >> 2, e);
      d(r.async), r.value.then(xe);
      var t = e + 128;
      Atomics.store((u(), M), t >> 2, 1);
    }
  }, xe = () => cr(() => {
    var e = ye();
    e && (fr(e), $t());
  }), wi = (e, r) => {
    if (e == r)
      setTimeout(xe);
    else if (S)
      postMessage({ targetThread: e, cmd: "checkMailbox" });
    else {
      var t = T.pthreads[e];
      if (!t) {
        k(`Cannot send message to thread with ID ${e}, unknown thread ID!`);
        return;
      }
      t.postMessage({ cmd: "checkMailbox" });
    }
  }, je = [], Ei = (e, r, t, n, a) => {
    n /= 2, je.length = n;
    for (var o = a >> 3, s = 0; s < n; s++)
      (u(), O)[o + 2 * s] ? je[s] = (u(), O)[o + 2 * s + 1] : je[s] = (u(), ke)[o + 2 * s + 1];
    var l = r ? vr[r] : Qi[e];
    d(!(e && r)), d(l.length == n, "Call args mismatch in _emscripten_receive_on_main_thread_js"), T.currentProxiedOperationCallerThread = t;
    var c = l(...je);
    return T.currentProxiedOperationCallerThread = 0, d(typeof c != "bigint"), c;
  }, Si = (e) => {
    S ? postMessage({ cmd: "cleanupThread", thread: e }) : Wr(e);
  }, bi = (e) => {
  }, ki = 9007199254740992, Ti = -9007199254740992, _r = (e) => e < Ti || e > ki ? NaN : Number(e);
  function wt(e, r, t, n, a, o, s) {
    if (S) return W(10, 0, 1, e, r, t, n, a, o, s);
    a = _r(a);
    try {
      d(!isNaN(a));
      var l = D.getStreamFromFD(n), c = i.mmap(l, e, a, r, t), _ = c.ptr;
      return (u(), M)[o >> 2] = c.allocated, (u(), y)[s >> 2] = _, 0;
    } catch (h) {
      if (typeof i > "u" || h.name !== "ErrnoError") throw h;
      return -h.errno;
    }
  }
  function Et(e, r, t, n, a, o) {
    if (S) return W(11, 0, 1, e, r, t, n, a, o);
    o = _r(o);
    try {
      var s = D.getStreamFromFD(a);
      t & 2 && D.doMsync(e, s, r, n, o);
    } catch (l) {
      if (typeof i > "u" || l.name !== "ErrnoError") throw l;
      return -l.errno;
    }
  }
  var Fi = (e, r, t, n) => {
    var a = (/* @__PURE__ */ new Date()).getFullYear(), o = new Date(a, 0, 1), s = new Date(a, 6, 1), l = o.getTimezoneOffset(), c = s.getTimezoneOffset(), _ = Math.max(l, c);
    (u(), y)[e >> 2] = _ * 60, (u(), M)[r >> 2] = +(l != c);
    var h = (g) => {
      var F = g >= 0 ? "-" : "+", R = Math.abs(g), A = String(Math.floor(R / 60)).padStart(2, "0"), N = String(R % 60).padStart(2, "0");
      return `UTC${F}${A}${N}`;
    }, w = h(l), p = h(c);
    d(w), d(p), d(se(w) <= 16, `timezone name truncated to fit in TZNAME_MAX (${w})`), d(se(p) <= 16, `timezone name truncated to fit in TZNAME_MAX (${p})`), c < l ? (Y(w, t, 17), Y(p, n, 17)) : (Y(w, n, 17), Y(p, t, 17));
  }, St = () => performance.timeOrigin + performance.now(), Ci = () => Date.now(), Ai = (e) => e >= 0 && e <= 3;
  function Pi(e, r, t) {
    if (!Ai(e))
      return 28;
    var n;
    e === 0 ? n = Ci() : n = St();
    var a = Math.round(n * 1e3 * 1e3);
    return (u(), O)[t >> 3] = BigInt(a), 0;
  }
  var Ue = [], Ii = (e, r) => {
    d(Array.isArray(Ue)), d(r % 16 == 0), Ue.length = 0;
    for (var t; t = (u(), U)[e++]; ) {
      var n = String.fromCharCode(t), a = ["d", "f", "i", "p"];
      a.push("j"), d(a.includes(n), `Invalid character ${t}("${n}") in readEmAsmArgs! Use only [${a}], and do not specify "v" for void return argument.`);
      var o = t != 105;
      o &= t != 112, r += o && r % 8 ? 4 : 0, Ue.push(t == 112 ? (u(), y)[r >> 2] : t == 106 ? (u(), O)[r >> 3] : t == 105 ? (u(), M)[r >> 2] : (u(), ke)[r >> 3]), r += o ? 8 : 4;
    }
    return Ue;
  }, Di = (e, r, t) => {
    var n = Ii(r, t);
    return d(vr.hasOwnProperty(e), `No EM_ASM constant found at address ${e}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), vr[e](...n);
  }, Ri = (e, r, t) => Di(e, r, t), Mi = () => {
    Z || ee("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
  }, Ni = (e, r) => k(K(e, r)), Oi = () => {
    throw ur(), "unwind";
  }, bt = () => 2147483648, Wi = () => bt(), Li = () => navigator.hardwareConcurrency, le = {}, kt = (e) => {
    var r = se(e) + 1, t = Ce(r);
    return t && Y(e, t, r), t;
  }, $e = (e) => {
    var r;
    if (r = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(e))
      return +r[1];
    if (r = /\bwasm-function\[(\d+)\]:(\d+)/.exec(e))
      ee("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
    else if (r = /:(\d+):\d+(?:\)|$)/.exec(e))
      return 2147483648 | +r[1];
    return 0;
  }, Tt = (e) => {
    for (var r of e) {
      var t = $e(r);
      t && (le[t] = r);
    }
  }, Ft = () => new Error().stack.toString(), xi = () => {
    var e = Ft().split(`
`);
    return e[0] == "Error" && e.shift(), Tt(e), le.last_addr = $e(e[3]), le.last_stack = e, le.last_addr;
  }, Be = (e) => {
    var r = le[e];
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
    return V(Be.ret ?? 0), Be.ret = kt(t), Be.ret;
  }, ji = (e) => {
    var r = z.buffer.byteLength, t = (e - r + 65535) / 65536 | 0;
    try {
      return z.grow(t), Re(), 1;
    } catch (n) {
      k(`growMemory: Attempted to grow heap from ${r} bytes to ${e} bytes, but got error: ${n}`);
    }
  }, Ui = (e) => {
    var r = (u(), U).length;
    if (e >>>= 0, e <= r)
      return !1;
    var t = bt();
    if (e > t)
      return k(`Cannot enlarge memory, requested ${e} bytes, but the limit is ${t} bytes!`), !1;
    for (var n = 1; n <= 4; n *= 2) {
      var a = r * (1 + 0.2 / n);
      a = Math.min(a, e + 100663296);
      var o = Math.min(t, Zr(Math.max(e, a), 65536)), s = ji(o);
      if (s)
        return !0;
    }
    return k(`Failed to grow the heap from ${r} bytes to ${o} bytes, not enough memory!`), !1;
  }, $i = (e, r, t) => {
    var n;
    le.last_addr == e ? n = le.last_stack : (n = Ft().split(`
`), n[0] == "Error" && n.shift(), Tt(n));
    for (var a = 3; n[a] && $e(n[a]) != e; )
      ++a;
    for (var o = 0; o < t && n[o + a]; ++o)
      (u(), M)[r + o * 4 >> 2] = $e(n[o + a]);
    return o;
  }, mr = {}, Bi = () => Sr || "./this.program", Fe = () => {
    if (!Fe.strings) {
      var e = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", r = { USER: "web_user", LOGNAME: "web_user", PATH: "/", PWD: "/", HOME: "/home/web_user", LANG: e, _: Bi() };
      for (var t in mr)
        mr[t] === void 0 ? delete r[t] : r[t] = mr[t];
      var n = [];
      for (var t in r)
        n.push(`${t}=${r[t]}`);
      Fe.strings = n;
    }
    return Fe.strings;
  };
  function Ct(e, r) {
    if (S) return W(12, 0, 1, e, r);
    var t = 0, n = 0;
    for (var a of Fe()) {
      var o = r + t;
      (u(), y)[e + n >> 2] = o, t += Y(a, o, 1 / 0) + 1, n += 4;
    }
    return 0;
  }
  function At(e, r) {
    if (S) return W(13, 0, 1, e, r);
    var t = Fe();
    (u(), y)[e >> 2] = t.length;
    var n = 0;
    for (var a of t)
      n += se(a) + 1;
    return (u(), y)[r >> 2] = n, 0;
  }
  function Pt(e) {
    if (S) return W(14, 0, 1, e);
    try {
      var r = D.getStreamFromFD(e);
      return i.close(r), 0;
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var zi = (e, r, t, n) => {
    for (var a = 0, o = 0; o < t; o++) {
      var s = (u(), y)[r >> 2], l = (u(), y)[r + 4 >> 2];
      r += 8;
      var c = i.read(e, (u(), L), s, l, n);
      if (c < 0) return -1;
      if (a += c, c < l) break;
    }
    return a;
  };
  function It(e, r, t, n) {
    if (S) return W(15, 0, 1, e, r, t, n);
    try {
      var a = D.getStreamFromFD(e), o = zi(a, r, t);
      return (u(), y)[n >> 2] = o, 0;
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function Dt(e, r, t, n) {
    if (S) return W(16, 0, 1, e, r, t, n);
    r = _r(r);
    try {
      if (isNaN(r)) return 61;
      var a = D.getStreamFromFD(e);
      return i.llseek(a, r, t), (u(), O)[n >> 3] = BigInt(a.position), a.getdents && r === 0 && t === 0 && (a.getdents = null), 0;
    } catch (o) {
      if (typeof i > "u" || o.name !== "ErrnoError") throw o;
      return o.errno;
    }
  }
  var Hi = (e, r, t, n) => {
    for (var a = 0, o = 0; o < t; o++) {
      var s = (u(), y)[r >> 2], l = (u(), y)[r + 4 >> 2];
      r += 8;
      var c = i.write(e, (u(), L), s, l, n);
      if (c < 0) return -1;
      if (a += c, c < l)
        break;
    }
    return a;
  };
  function Rt(e, r, t, n) {
    if (S) return W(17, 0, 1, e, r, t, n);
    try {
      var a = D.getStreamFromFD(e), o = Hi(a, r, t);
      return (u(), y)[n >> 2] = o, 0;
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function Vi(e, r) {
    try {
      return ar((u(), U).subarray(e, e + r)), 0;
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var Gi = (e) => {
    var r = f["_" + e];
    return d(r, "Cannot call unknown function " + e + ", make sure it is exported"), r;
  }, qi = (e, r) => {
    d(e.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (u(), L).set(e, r);
  }, Ki = (e) => {
    var r = se(e) + 1, t = rr(r);
    return Y(e, t, r), t;
  }, Mt = (e, r, t, n, a) => {
    var o = { string: (A) => {
      var N = 0;
      return A != null && A !== 0 && (N = Ki(A)), N;
    }, array: (A) => {
      var N = rr(A.length);
      return qi(A, N), N;
    } };
    function s(A) {
      return r === "string" ? K(A) : r === "boolean" ? !!A : A;
    }
    var l = Gi(e), c = [], _ = 0;
    if (d(r !== "array", 'Return type should not be "array".'), n)
      for (var h = 0; h < n.length; h++) {
        var w = o[t[h]];
        w ? (_ === 0 && (_ = zr()), c[h] = w(n[h])) : c[h] = n[h];
      }
    var p = m.currData, g = l(...c);
    function F(A) {
      return ht(), _ !== 0 && er(_), s(A);
    }
    var R = a?.async;
    return ur(), m.currData != p ? (d(!(p && m.currData), "We cannot start an async operation when one is already flight"), d(!(p && !m.currData), "We cannot stop an async operation in flight"), d(R, "The call to " + e + " is running asynchronously. If this was intended, add the async option to the ccall/cwrap call."), m.whenDone().then(F)) : (g = F(g), R ? Promise.resolve(g) : g);
  }, Xi = (e, r, t, n) => (...a) => Mt(e, r, t, a, n), Yi = (...e) => kt(...e);
  T.init(), i.createPreloadedFile = Rn, i.preloadFile = tt, i.staticInit(), d(X.length === 10);
  {
    if (an(), f.noExitRuntime && (ir = f.noExitRuntime), f.preloadPlugins && (rt = f.preloadPlugins), f.print && (Q = f.print), f.printErr && (k = f.printErr), f.wasmBinary && (Ee = f.wasmBinary), ea(), f.arguments && f.arguments, f.thisProgram && (Sr = f.thisProgram), d(typeof f.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), d(typeof f.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), d(typeof f.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), d(typeof f.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), d(typeof f.read > "u", "Module.read option was removed"), d(typeof f.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), d(typeof f.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), d(typeof f.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), d(typeof f.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), d(typeof f.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), d(typeof f.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), f.preInit)
      for (typeof f.preInit == "function" && (f.preInit = [f.preInit]); f.preInit.length > 0; )
        f.preInit.shift()();
    De("preInit");
  }
  f.ccall = Mt, f.cwrap = Xi, f.UTF8ToString = K, f.stringToUTF8 = Y, f.allocateUTF8 = Yi;
  var Ji = ["writeI53ToI64", "writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "convertU32PairToI53", "getTempRet0", "setTempRet0", "withStackSave", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "runMainThreadEmAsm", "jstoi_q", "autoResumeAudioContext", "asmjsMangle", "HandleAllocator", "addOnInit", "addOnPostCtor", "addOnPreMain", "addOnExit", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "intArrayToString", "stringToAscii", "registerKeyEventCallback", "maybeCStringToJsString", "findEventTarget", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "registerBatteryEventCallback", "setCanvasElementSizeCallingThread", "setCanvasElementSizeMainThread", "setCanvasElementSize", "getCanvasSizeCallingThread", "getCanvasSizeMainThread", "getCanvasElementSize", "getCallstack", "convertPCtoSourceLocation", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "safeSetTimeout", "setImmediateWrapped", "safeRequestAnimationFrame", "clearImmediateWrapped", "registerPostMainLoop", "registerPreMainLoop", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "isLeapYear", "ydayFromDate", "arraySum", "addDays", "getSocketFromFD", "getSocketAddress", "FS_mkdirTree", "_setNetworkCallback", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "webgl_enable_EXT_polygon_offset_clamp", "webgl_enable_EXT_clip_control", "webgl_enable_WEBGL_polygon_mode", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "emscriptenWebGLGetUniform", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "emscripten_webgl_destroy_context_before_on_calling_thread", "registerWebGlEventCallback", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "allocateUTF8OnStack", "demangle", "stackTrace", "getNativeTypeSize", "getFunctionArgsName", "requireRegisteredType", "createJsInvokerSignature", "PureVirtualError", "getBasestPointer", "registerInheritedInstance", "unregisterInheritedInstance", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "enumReadValueFromPointer", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "flushPendingDeletes", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "count_emval_handles", "getStringOrSymbol", "emval_returnValue", "emval_lookupTypes", "emval_addMethodCaller"];
  Ji.forEach(tn);
  var Zi = ["run", "out", "err", "callMain", "abort", "wasmExports", "HEAPF32", "HEAPF64", "HEAP8", "HEAP16", "HEAPU16", "HEAP32", "HEAPU32", "HEAP64", "HEAPU64", "writeStackCookie", "checkStackCookie", "INT53_MAX", "INT53_MIN", "bigintToI53Checked", "stackSave", "stackRestore", "stackAlloc", "createNamedFunction", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "ERRNO_CODES", "strError", "DNS", "Protocols", "Sockets", "timers", "warnOnce", "readEmAsmArgsArray", "readEmAsmArgs", "runEmAsmFunction", "getExecutableName", "dynCallLegacy", "getDynCaller", "dynCall", "handleException", "keepRuntimeAlive", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "wasmMemory", "getUniqueRunDependency", "noExitRuntime", "addRunDependency", "removeRunDependency", "addOnPreRun", "addOnPostRun", "freeTableIndexes", "functionsInTableMap", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "stringToUTF8Array", "lengthBytesUTF8", "intArrayFromString", "AsciiToString", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "stringToUTF8OnStack", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "jsStackTrace", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "checkWasiClock", "doReadv", "doWritev", "initRandomFill", "randomFill", "emSetImmediate", "emClearImmediate_deps", "emClearImmediate", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "requestFullscreen", "requestFullScreen", "setCanvasSize", "getUserMedia", "createContext", "getPreloadedImageData__data", "wget", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_preloadFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS_unlink", "FS_createPath", "FS_createDevice", "FS_readFile", "FS", "FS_root", "FS_mounts", "FS_devices", "FS_streams", "FS_nextInode", "FS_nameTable", "FS_currentPath", "FS_initialized", "FS_ignorePermissions", "FS_filesystems", "FS_syncFSRequests", "FS_readFiles", "FS_lookupPath", "FS_getPath", "FS_hashName", "FS_hashAddNode", "FS_hashRemoveNode", "FS_lookupNode", "FS_createNode", "FS_destroyNode", "FS_isRoot", "FS_isMountpoint", "FS_isFile", "FS_isDir", "FS_isLink", "FS_isChrdev", "FS_isBlkdev", "FS_isFIFO", "FS_isSocket", "FS_flagsToPermissionString", "FS_nodePermissions", "FS_mayLookup", "FS_mayCreate", "FS_mayDelete", "FS_mayOpen", "FS_checkOpExists", "FS_nextfd", "FS_getStreamChecked", "FS_getStream", "FS_createStream", "FS_closeStream", "FS_dupStream", "FS_doSetAttr", "FS_chrdev_stream_ops", "FS_major", "FS_minor", "FS_makedev", "FS_registerDevice", "FS_getDevice", "FS_getMounts", "FS_syncfs", "FS_mount", "FS_unmount", "FS_lookup", "FS_mknod", "FS_statfs", "FS_statfsStream", "FS_statfsNode", "FS_create", "FS_mkdir", "FS_mkdev", "FS_symlink", "FS_rename", "FS_rmdir", "FS_readdir", "FS_readlink", "FS_stat", "FS_fstat", "FS_lstat", "FS_doChmod", "FS_chmod", "FS_lchmod", "FS_fchmod", "FS_doChown", "FS_chown", "FS_lchown", "FS_fchown", "FS_doTruncate", "FS_truncate", "FS_ftruncate", "FS_utime", "FS_open", "FS_close", "FS_isClosed", "FS_llseek", "FS_read", "FS_write", "FS_mmap", "FS_msync", "FS_ioctl", "FS_writeFile", "FS_cwd", "FS_chdir", "FS_createDefaultDirectories", "FS_createDefaultDevices", "FS_createSpecialDirectories", "FS_createStandardStreams", "FS_staticInit", "FS_init", "FS_quit", "FS_findObject", "FS_analyzePath", "FS_createFile", "FS_createDataFile", "FS_forceLoadFile", "FS_createLazyFile", "FS_absolutePath", "FS_createFolder", "FS_createLink", "FS_joinPath", "FS_mmapAlloc", "FS_standardizePath", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "GL", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "runAndAbortIfError", "Asyncify", "Fibers", "SDL", "SDL_gfx", "print", "printErr", "jstoi_s", "PThread", "terminateWorker", "cleanupThread", "registerTLSInit", "spawnThread", "exitOnMainThread", "proxyToMainThread", "proxiedJSCallArgs", "invokeEntryPoint", "checkMailbox", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "getTypeName", "getFunctionName", "heap32VectorToArray", "usesDestructorStack", "checkArgCount", "getRequiredArgCount", "createJsInvoker", "UnboundTypeError", "EmValType", "EmValOptionalType", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "embindRepr", "registeredInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "floatReadValueFromPointer", "assertIntegerRange", "readPointer", "runDestructors", "craftInvokerFunction", "embind__requireFunction", "finalizationRegistry", "detachFinalizer_deps", "deletionQueue", "delayFunction", "emval_freelist", "emval_handles", "emval_symbols", "Emval", "emval_methodCallers"];
  Zi.forEach(Fr);
  var Qi = [tr, Hr, Xr, nt, it, at, ot, st, lt, dt, wt, Et, Ct, At, Pt, It, Dt, Rt];
  function ea() {
    en("fetchSettings");
  }
  var vr = { 540932: () => typeof wasmOffsetConverter < "u" };
  function ra() {
    return typeof wasmOffsetConverter < "u";
  }
  var Nt = C("___getTypeName"), Ot = C("__embind_initialize_bindings");
  f._get_cp_model_schema = C("_get_cp_model_schema"), f._get_sat_parameters_schema = C("_get_sat_parameters_schema"), f._solve_model = C("_solve_model");
  var Ce = f._malloc = C("_malloc");
  f._free_buffer = C("_free_buffer");
  var V = f._free = C("_free");
  f._interrupt_solve = C("_interrupt_solve"), f._validate_model = C("_validate_model");
  var ye = C("_pthread_self"), pr = C("_fflush"), Wt = C("_strerror"), Lt = C("_emscripten_builtin_memalign"), hr = C("__emscripten_thread_init"), xt = C("__emscripten_thread_crashed"), yr = C("_emscripten_stack_get_end"), jt = C("__emscripten_run_js_on_main_thread"), Ut = C("__emscripten_thread_free_data"), gr = C("__emscripten_thread_exit"), $t = C("__emscripten_check_mailbox"), Bt = C("_emscripten_stack_init"), zt = C("_emscripten_stack_set_limits"), Ht = C("__emscripten_stack_restore"), Vt = C("__emscripten_stack_alloc"), wr = C("_emscripten_stack_get_current"), Gt = C("dynCall_ii"), qt = C("_asyncify_start_unwind"), Kt = C("_asyncify_stop_unwind"), Xt = C("_asyncify_start_rewind"), Yt = C("_asyncify_stop_rewind");
  function ta(e) {
    d(typeof e.__getTypeName < "u", "missing Wasm export: __getTypeName"), d(typeof e._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), d(typeof e.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), d(typeof e.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), d(typeof e.solve_model < "u", "missing Wasm export: solve_model"), d(typeof e.malloc < "u", "missing Wasm export: malloc"), d(typeof e.free_buffer < "u", "missing Wasm export: free_buffer"), d(typeof e.free < "u", "missing Wasm export: free"), d(typeof e.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), d(typeof e.validate_model < "u", "missing Wasm export: validate_model"), d(typeof e.pthread_self < "u", "missing Wasm export: pthread_self"), d(typeof e.fflush < "u", "missing Wasm export: fflush"), d(typeof e.strerror < "u", "missing Wasm export: strerror"), d(typeof e._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), d(typeof e.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), d(typeof e._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), d(typeof e._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), d(typeof e.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), d(typeof e.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), d(typeof e._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), d(typeof e._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), d(typeof e._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), d(typeof e._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), d(typeof e.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), d(typeof e.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), d(typeof e.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), d(typeof e._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), d(typeof e._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), d(typeof e.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), d(typeof e.dynCall_ii < "u", "missing Wasm export: dynCall_ii"), d(typeof e.dynCall_v < "u", "missing Wasm export: dynCall_v"), d(typeof e.dynCall_vi < "u", "missing Wasm export: dynCall_vi"), d(typeof e.dynCall_iii < "u", "missing Wasm export: dynCall_iii"), d(typeof e.dynCall_iiii < "u", "missing Wasm export: dynCall_iiii"), d(typeof e.dynCall_vii < "u", "missing Wasm export: dynCall_vii"), d(typeof e.dynCall_iiiijij < "u", "missing Wasm export: dynCall_iiiijij"), d(typeof e.dynCall_viii < "u", "missing Wasm export: dynCall_viii"), d(typeof e.dynCall_iiiii < "u", "missing Wasm export: dynCall_iiiii"), d(typeof e.dynCall_viiii < "u", "missing Wasm export: dynCall_viiii"), d(typeof e.dynCall_viiiiii < "u", "missing Wasm export: dynCall_viiiiii"), d(typeof e.dynCall_viiiii < "u", "missing Wasm export: dynCall_viiiii"), d(typeof e.dynCall_viiiij < "u", "missing Wasm export: dynCall_viiiij"), d(typeof e.dynCall_ji < "u", "missing Wasm export: dynCall_ji"), d(typeof e.dynCall_viij < "u", "missing Wasm export: dynCall_viij"), d(typeof e.dynCall_i < "u", "missing Wasm export: dynCall_i"), d(typeof e.dynCall_di < "u", "missing Wasm export: dynCall_di"), d(typeof e.dynCall_viijiiii < "u", "missing Wasm export: dynCall_viijiiii"), d(typeof e.dynCall_jii < "u", "missing Wasm export: dynCall_jii"), d(typeof e.dynCall_viji < "u", "missing Wasm export: dynCall_viji"), d(typeof e.dynCall_vifi < "u", "missing Wasm export: dynCall_vifi"), d(typeof e.dynCall_vidi < "u", "missing Wasm export: dynCall_vidi"), d(typeof e.dynCall_viiiiiii < "u", "missing Wasm export: dynCall_viiiiiii"), d(typeof e.dynCall_iiiiiii < "u", "missing Wasm export: dynCall_iiiiiii"), d(typeof e.dynCall_viiji < "u", "missing Wasm export: dynCall_viiji"), d(typeof e.dynCall_jiiji < "u", "missing Wasm export: dynCall_jiiji"), d(typeof e.dynCall_dij < "u", "missing Wasm export: dynCall_dij"), d(typeof e.dynCall_dii < "u", "missing Wasm export: dynCall_dii"), d(typeof e.dynCall_fii < "u", "missing Wasm export: dynCall_fii"), d(typeof e.dynCall_viijii < "u", "missing Wasm export: dynCall_viijii"), d(typeof e.dynCall_iiiiii < "u", "missing Wasm export: dynCall_iiiiii"), d(typeof e.dynCall_vj < "u", "missing Wasm export: dynCall_vj"), d(typeof e.dynCall_vij < "u", "missing Wasm export: dynCall_vij"), d(typeof e.dynCall_jiji < "u", "missing Wasm export: dynCall_jiji"), d(typeof e.dynCall_iidiiii < "u", "missing Wasm export: dynCall_iidiiii"), d(typeof e.dynCall_iiiiiiiii < "u", "missing Wasm export: dynCall_iiiiiiiii"), d(typeof e.dynCall_iiiiij < "u", "missing Wasm export: dynCall_iiiiij"), d(typeof e.dynCall_iiiiid < "u", "missing Wasm export: dynCall_iiiiid"), d(typeof e.dynCall_iiiiijj < "u", "missing Wasm export: dynCall_iiiiijj"), d(typeof e.dynCall_iiiiiiii < "u", "missing Wasm export: dynCall_iiiiiiii"), d(typeof e.dynCall_iiiiiijj < "u", "missing Wasm export: dynCall_iiiiiijj"), d(typeof e.asyncify_start_unwind < "u", "missing Wasm export: asyncify_start_unwind"), d(typeof e.asyncify_stop_unwind < "u", "missing Wasm export: asyncify_stop_unwind"), d(typeof e.asyncify_start_rewind < "u", "missing Wasm export: asyncify_start_rewind"), d(typeof e.asyncify_stop_rewind < "u", "missing Wasm export: asyncify_stop_rewind"), d(typeof e.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), Nt = v("__getTypeName", 1), Ot = v("_embind_initialize_bindings", 0), f._get_cp_model_schema = v("get_cp_model_schema", 0), f._get_sat_parameters_schema = v("get_sat_parameters_schema", 0), f._solve_model = v("solve_model", 5), Ce = f._malloc = v("malloc", 1), f._free_buffer = v("free_buffer", 1), V = f._free = v("free", 1), f._interrupt_solve = v("interrupt_solve", 0), f._validate_model = v("validate_model", 3), ye = v("pthread_self", 0), pr = v("fflush", 1), Wt = v("strerror", 1), Lt = v("emscripten_builtin_memalign", 2), hr = v("_emscripten_thread_init", 6), xt = v("_emscripten_thread_crashed", 0), yr = e.emscripten_stack_get_end, e.emscripten_stack_get_base, jt = v("_emscripten_run_js_on_main_thread", 5), Ut = v("_emscripten_thread_free_data", 1), gr = v("_emscripten_thread_exit", 1), $t = v("_emscripten_check_mailbox", 0), Bt = e.emscripten_stack_init, zt = e.emscripten_stack_set_limits, e.emscripten_stack_get_free, Ht = e._emscripten_stack_restore, Vt = e._emscripten_stack_alloc, wr = e.emscripten_stack_get_current, Gt = E.ii = v("dynCall_ii", 2), E.v = v("dynCall_v", 1), E.vi = v("dynCall_vi", 2), E.iii = v("dynCall_iii", 3), E.iiii = v("dynCall_iiii", 4), E.vii = v("dynCall_vii", 3), E.iiiijij = v("dynCall_iiiijij", 7), E.viii = v("dynCall_viii", 4), E.iiiii = v("dynCall_iiiii", 5), E.viiii = v("dynCall_viiii", 5), E.viiiiii = v("dynCall_viiiiii", 7), E.viiiii = v("dynCall_viiiii", 6), E.viiiij = v("dynCall_viiiij", 6), E.ji = v("dynCall_ji", 2), E.viij = v("dynCall_viij", 4), E.i = v("dynCall_i", 1), E.di = v("dynCall_di", 2), E.viijiiii = v("dynCall_viijiiii", 8), E.jii = v("dynCall_jii", 3), E.viji = v("dynCall_viji", 4), E.vifi = v("dynCall_vifi", 4), E.vidi = v("dynCall_vidi", 4), E.viiiiiii = v("dynCall_viiiiiii", 8), E.iiiiiii = v("dynCall_iiiiiii", 7), E.viiji = v("dynCall_viiji", 5), E.jiiji = v("dynCall_jiiji", 5), E.dij = v("dynCall_dij", 3), E.dii = v("dynCall_dii", 3), E.fii = v("dynCall_fii", 3), E.viijii = v("dynCall_viijii", 6), E.iiiiii = v("dynCall_iiiiii", 6), E.vj = v("dynCall_vj", 2), E.vij = v("dynCall_vij", 3), E.jiji = v("dynCall_jiji", 4), E.iidiiii = v("dynCall_iidiiii", 7), E.iiiiiiiii = v("dynCall_iiiiiiiii", 9), E.iiiiij = v("dynCall_iiiiij", 6), E.iiiiid = v("dynCall_iiiiid", 6), E.iiiiijj = v("dynCall_iiiiijj", 7), E.iiiiiiii = v("dynCall_iiiiiiii", 8), E.iiiiiijj = v("dynCall_iiiiiijj", 8), qt = v("asyncify_start_unwind", 1), Kt = v("asyncify_stop_unwind", 0), Xt = v("asyncify_start_rewind", 1), Yt = v("asyncify_stop_rewind", 0), e.__indirect_function_table;
  }
  var ge;
  function na() {
    ge = { HaveOffsetConverter: ra, __assert_fail: gn, __cxa_throw: En, __pthread_create_js: Yr, __syscall_fcntl64: nt, __syscall_fstat64: it, __syscall_ioctl: at, __syscall_lstat64: ot, __syscall_newfstatat: st, __syscall_openat: lt, __syscall_stat64: dt, _abort_js: Mn, _embind_register_bigint: Wn, _embind_register_bool: Ln, _embind_register_emval: Un, _embind_register_float: Bn, _embind_register_function: si, _embind_register_integer: li, _embind_register_memory_view: di, _embind_register_std_string: ci, _embind_register_std_wstring: hi, _embind_register_void: yi, _emscripten_init_main_thread_js: gi, _emscripten_notify_mailbox_postmessage: wi, _emscripten_receive_on_main_thread_js: Ei, _emscripten_thread_cleanup: Si, _emscripten_thread_mailbox_await: fr, _emscripten_thread_set_strongref: bi, _mmap_js: wt, _munmap_js: Et, _tzset_js: Fi, clock_time_get: Pi, emscripten_asm_const_int: Ri, emscripten_check_blocking_allowed: Mi, emscripten_errn: Ni, emscripten_exit_with_live_runtime: Oi, emscripten_get_heap_max: Wi, emscripten_get_now: St, emscripten_num_logical_cores: Li, emscripten_pc_get_function: Be, emscripten_resize_heap: Ui, emscripten_stack_snapshot: xi, emscripten_stack_unwind_buffer: $i, environ_get: Ct, environ_sizes_get: At, exit: nr, fd_close: Pt, fd_read: It, fd_seek: Dt, fd_write: Rt, memory: z, proc_exit: tr, random_get: Vi };
  }
  var Jt;
  function ia() {
    d(!S), Bt(), Tr();
  }
  function ze() {
    if (ie > 0) {
      Te = ze;
      return;
    }
    if (S) {
      Ye?.(f), Dr();
      return;
    }
    if (ia(), on(), ie > 0) {
      Te = ze;
      return;
    }
    function e() {
      d(!Jt), Jt = !0, f.calledRun = !0, !q && (Dr(), Ye?.(f), f.onRuntimeInitialized?.(), De("onRuntimeInitialized"), d(!f._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), sn());
    }
    f.setStatus ? (f.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => f.setStatus(""), 1), e();
    }, 1)) : e(), Se();
  }
  function aa() {
    var e = Q, r = k, t = !1;
    Q = k = (c) => {
      t = !0;
    };
    try {
      pr(0);
      for (var n of ["stdout", "stderr"]) {
        var a = i.analyzePath("/dev/" + n);
        if (!a) return;
        var o = a.object, s = o.rdev, l = re.ttys[s];
        l?.output?.length && (t = !0);
      }
    } catch {
    }
    Q = e, k = r, t && ee("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
  }
  var J;
  S || (J = await Mr(), ze()), ce ? Ve = f : Ve = new Promise((e, r) => {
    Ye = e, Je = r;
  });
  for (const e of Object.keys(f))
    e in He || Object.defineProperty(He, e, { configurable: !0, get() {
      P(`Access to module property ('${e}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
    } });
  return Ve;
}
var sa = globalThis.self?.name?.startsWith("em-pthread");
sa && oa();
//# sourceMappingURL=cp_sat_runtime_asyncify-DVtiNoln.js.map
