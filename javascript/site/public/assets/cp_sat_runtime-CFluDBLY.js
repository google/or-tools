async function ea(He = {}) {
  var Ve;
  (function() {
    function e(d) {
      d = d.split("-")[0];
      for (var m = d.split(".").slice(0, 3); m.length < 3; ) m.push("00");
      return m = m.map((v, g, _) => v.padStart(2, "0")), m.join("");
    }
    var r = (d) => [d / 1e4 | 0, (d / 100 | 0) % 100, d % 100].join("."), t = 2147483647, n = typeof process < "u" && process.versions?.node ? e(process.versions.node) : t;
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
  var f = He, Ie = !!globalThis.window, ee = !!globalThis.WorkerGlobalScope, Ge = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", gr = !Ie && !Ge && !ee, y = ee && self.name?.startsWith("em-pthread");
  y && (c(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0);
  var yr = "./this.program", Er = (e, r) => {
    throw r;
  }, Xt = import.meta.url, Re = "";
  function Yt(e) {
    return b("locateFile:", e, "scriptDirectory:", Re), f.locateFile ? f.locateFile(e, Re) : Re + e;
  }
  var qe, De;
  if (!gr) if (Ie || ee) {
    try {
      Re = new URL(".", Xt).href;
    } catch {
    }
    if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    ee && (De = (e) => {
      var r = new XMLHttpRequest();
      return r.open("GET", e, !1), r.responseType = "arraybuffer", r.send(null), new Uint8Array(r.response);
    }), qe = async (e) => {
      c(!wr(e), "readAsync does not work with file:// URLs");
      var r = await fetch(e, { credentials: "same-origin" });
      if (r.ok)
        return r.arrayBuffer();
      throw new Error(r.status + " : " + r.url);
    };
  } else
    throw new Error("environment detection error");
  var re = console.log.bind(console), w = console.error.bind(console);
  c(Ie || ee || Ge, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), c(!Ge, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable."), c(!gr, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
  var we;
  globalThis.WebAssembly || w("no native wasm support detected");
  var oe, ue = !1, te;
  function c(e, r) {
    e || P("Assertion failed" + (r ? ": " + r : ""));
  }
  var wr = (e) => e.startsWith("file://");
  function br() {
    var e = vr();
    b(`writeStackCookie: ${W(e)}`), c((e & 3) == 0), e == 0 && (e += 4), (u(), h)[e >> 2] = 34821223, (u(), h)[e + 4 >> 2] = 2310721022, (u(), h)[0] = 1668509029;
  }
  function be() {
    if (!ue) {
      var e = vr();
      e == 0 && (e += 4);
      var r = (u(), h)[e >> 2], t = (u(), h)[e + 4 >> 2];
      (r != 34821223 || t != 2310721022) && P(`Stack overflow! Stack cookie has been overwritten at ${W(e)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${W(t)} ${W(r)}`), (u(), h)[0] != 1668509029 && P("Runtime error: The application has corrupted its heap memory area (address zero)!");
    }
  }
  function b(...e) {
    console.warn(...e);
  }
  (() => {
    var e = new Int16Array(1), r = new Int8Array(e.buffer);
    e[0] = 25459, (r[0] !== 115 || r[1] !== 99) && P("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
  })();
  function Me(e) {
    Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, { configurable: !0, set() {
      P(`Attempt to set \`Module.${e}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
    } });
  }
  function A(e) {
    return () => c(!1, `call to '${e}' via reference taken before Wasm module initialization`);
  }
  function Jt(e) {
    Object.getOwnPropertyDescriptor(f, e) && P(`\`Module.${e}\` was supplied but \`${e}\` not included in INCOMING_MODULE_JS_API`);
  }
  function Zt(e) {
    return e === "FS_createPath" || e === "FS_createDataFile" || e === "FS_createPreloadedFile" || e === "FS_preloadFile" || e === "FS_unlink" || e === "addRunDependency" || e === "FS_createLazyFile" || e === "FS_createDevice" || e === "removeRunDependency";
  }
  function Qt(e) {
    Sr(e);
  }
  function Sr(e) {
    y || Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, { configurable: !0, get() {
      var r = `'${e}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
      Zt(e) && (r += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), P(r);
    } });
  }
  function en() {
    function e() {
      var n = 0;
      return fe && typeof Z < "u" && (n = Z()), `w:${kr},t:${W(n)}:`;
    }
    var r = b;
    b = (...n) => r(e(), ...n);
    var t = w;
    w = (...n) => t(e(), ...n);
  }
  en();
  function u() {
    j.buffer != O.buffer && Ne();
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
          kr = t.workerID, b("worker: loading module");
          let a = [];
          self.onmessage = (o) => a.push(o), Tr = () => {
            postMessage({ cmd: "loaded" });
            for (let o of a)
              e(o);
            self.onmessage = e;
          };
          for (const o of t.handlers)
            !f[o] || f[o].proxy ? (b(`worker: installer proxying handler: ${o}`), f[o] = (...s) => {
              b(`worker: calling handler on main thread: ${o}`), postMessage({ cmd: "callHandler", handler: o, args: s });
            }, o == "print" && (re = f[o]), o == "printErr" && (w = f[o])) : b(`worker: using thread-local handler: ${o}`);
          j = t.wasmMemory, Ne(), oe = t.wasmModule, Ir(), je();
        } else if (n === "run") {
          c(t.pthread_ptr), fn(t.pthread_ptr), _r(t.pthread_ptr, 0, 0, 1, 0, 0), k.threadInitTLS(), dr(t.pthread_ptr), Ye || (b(`worker: Pthread 0x${Z().toString(16)} initializing embind.`), xt(), Ye = !0);
          try {
            await Hr(t.start_routine, t.arg);
          } catch (a) {
            if (a != "unwind")
              throw a;
            b(`worker: Pthread 0x${Z().toString(16)} completed its main entry point with an 'unwind', keeping the worker alive for asynchronous operation.`);
          }
        } else t.target === "setimmediate" || (n === "checkMailbox" ? Ye && Le() : n && (w(`worker: received unknown command ${n}`), w(t)));
      } catch (a) {
        throw w(`worker: onmessage() captured an uncaught exception: ${a}`), a?.stack && w(a.stack), Ut(), a;
      }
    }
    self.onmessage = e;
  }
  var O, $, se, Se, I, h, Fr, ke, D, Ar, fe = !1;
  function Ne() {
    var e = j.buffer;
    O = new Int8Array(e), se = new Int16Array(e), f.HEAPU8 = $ = new Uint8Array(e), Se = new Uint16Array(e), I = new Int32Array(e), h = new Uint32Array(e), Fr = new Float32Array(e), ke = new Float64Array(e), D = new BigInt64Array(e), Ar = new BigUint64Array(e);
  }
  function rn() {
    if (!y) {
      if (f.wasmMemory)
        j = f.wasmMemory;
      else {
        var e = f.INITIAL_MEMORY || 16777216;
        c(e >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + e + "! (STACK_SIZE=65536)"), j = new WebAssembly.Memory({ initial: e / 65536, maximum: 32768, shared: !0 });
      }
      Ne();
    }
  }
  c(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
  function tn() {
    if (c(!y), f.preRun)
      for (typeof f.preRun == "function" && (f.preRun = [f.preRun]); f.preRun.length; )
        xr(f.preRun.shift());
    Me("preRun"), Nr(Or);
  }
  function Pr() {
    if (b("initRuntime"), c(!fe), fe = !0, y) return Tr();
    be(), !f.noFSInit && !i.initialized && i.init(), Q.__wasm_call_ctors(), b("done __wasm_call_ctors"), i.ignorePermissions = !1, b("done ATPOSTCTORS");
  }
  function nn() {
    if (be(), !y) {
      if (f.postRun)
        for (typeof f.postRun == "function" && (f.postRun = [f.postRun]); f.postRun.length; )
          un(f.postRun.shift());
      Me("postRun"), Nr(zr);
    }
  }
  function P(e) {
    f.onAbort?.(e), e = "Aborted(" + e + ")", w(e), ue = !0;
    var r = new WebAssembly.RuntimeError(e);
    throw Xe?.(r), r;
  }
  function M(e, r) {
    return (...t) => {
      c(fe, `native function \`${e}\` called before runtime initialization`);
      var n = Q[e];
      return c(n, `exported native function \`${e}\` not found`), c(t.length <= r, `native function \`${e}\` called with ${t.length} args but expects ${r}`), n(...t);
    };
  }
  var Je;
  function an() {
    return f.locateFile ? Yt("cp_sat_runtime.wasm") : new URL("/assets/cp_sat_runtime-DSuW0fwz.wasm", import.meta.url).href;
  }
  function on(e) {
    if (e == Je && we)
      return new Uint8Array(we);
    if (De)
      return De(e);
    throw "both async and sync fetching of the wasm failed";
  }
  async function sn(e) {
    if (!we)
      try {
        var r = await qe(e);
        return new Uint8Array(r);
      } catch {
      }
    return on(e);
  }
  async function ln(e, r) {
    try {
      var t = await sn(e), n = await WebAssembly.instantiate(t, r);
      return n;
    } catch (a) {
      w(`failed to asynchronously prepare wasm: ${a}`), wr(e) && w(`warning: Loading from a file URI (${e}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), P(a);
    }
  }
  async function dn(e, r, t) {
    if (!e)
      try {
        var n = fetch(r, { credentials: "same-origin" }), a = await WebAssembly.instantiateStreaming(n, t);
        return a;
      } catch (o) {
        w(`wasm streaming compile failed: ${o}`), w("falling back to ArrayBuffer instantiation");
      }
    return ln(r, t);
  }
  function Cr() {
    Ji(), ye.__instrumented || (ye.__instrumented = !0, G.instrumentWasmImports(ye));
    var e = { env: ye, wasi_snapshot_preview1: ye };
    return e;
  }
  async function Ir() {
    function e(l, d) {
      return b("receiveInstance"), Q = l.exports, Q = G.instrumentWasmExports(Q), mn(Q._emscripten_tls_init), b("assigning exports"), Yi(Q), oe = d, Q;
    }
    var r = f;
    function t(l) {
      return c(f === r, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), r = null, e(l.instance, l.module);
    }
    var n = Cr();
    if (f.instantiateWasm)
      return new Promise((l, d) => {
        try {
          f.instantiateWasm(n, (m, v) => {
            l(e(m, v));
          });
        } catch (m) {
          w(`Module.instantiateWasm callback failed with error: ${m}`), d(m);
        }
      });
    if (y) {
      c(oe, "wasmModule should have been received via postMessage");
      var a = new WebAssembly.Instance(oe, Cr());
      return e(a, oe);
    }
    Je ??= an(), b("asynchronously preparing wasm");
    var o = await dn(we, Je, n), s = t(o);
    return s;
  }
  class Rr {
    name = "ExitStatus";
    constructor(r) {
      this.message = `Program terminated with exit(${r})`, this.status = r;
    }
  }
  var Dr = (e) => {
    b(`terminateWorker: ${e.workerID}`), e.terminate(), e.onmessage = (r) => {
      var t = r.data.cmd;
      w(`received "${t}" command from terminated worker: ${e.workerID}`);
    };
  }, Mr = (e) => {
    b(`cleanupThread: ${W(e)}`), c(!y, "Internal Error! cleanupThread() can only ever be called from main application thread!"), c(e, "Internal Error! Null pthread_ptr in cleanupThread!");
    var r = k.pthreads[e];
    c(r), k.returnWorkerToPool(r);
  }, Nr = (e) => {
    for (; e.length > 0; )
      e.shift()(f);
  }, Or = [], xr = (e) => Or.push(e), le = 0, Te = null, me = {}, de = null, Wr = (e) => {
    if (le--, f.monitorRunDependencies?.(le), b("removeRunDependency", e), c(e, "removeRunDependency requires an ID"), c(me[e]), delete me[e], le == 0 && (de !== null && (clearInterval(de), de = null), Te)) {
      var r = Te;
      Te = null, r();
    }
  }, Lr = (e) => {
    le++, f.monitorRunDependencies?.(le), b("addRunDependency", e), c(e, "addRunDependency requires an ID"), c(!me[e]), me[e] = 1, de === null && globalThis.setInterval && (de = setInterval(() => {
      if (ue) {
        clearInterval(de), de = null;
        return;
      }
      var r = !1;
      for (var t in me)
        r || (r = !0, w("still waiting on run dependencies:")), w(`dependency: ${t}`);
      r && w("(end of list)");
    }, 1e4));
  }, Ur = (e) => {
    c(!y, "Internal Error! spawnThread() can only ever be called from main application thread!"), c(e.pthread_ptr, "Internal error, no pthread ptr!");
    var r = k.getNewWorker();
    if (!r)
      return 6;
    c(!r.pthread_ptr, "Internal error!"), k.runningWorkers.push(r), k.pthreads[e.pthread_ptr] = r, r.pthread_ptr = e.pthread_ptr;
    var t = { cmd: "run", start_routine: e.startRoutine, arg: e.arg, pthread_ptr: e.pthread_ptr };
    return r.postMessage(t, e.transferList), 0;
  }, K = 0, Fe = () => nr || K > 0, $r = () => pr(), Ze = (e) => Vt(e), Qe = (e) => Gt(e), N = (e, r, t, ...n) => {
    for (var a = n.length * 2, o = $r(), s = Qe(a * 8), l = s >> 3, d = 0; d < n.length; d++) {
      var m = n[d];
      typeof m == "bigint" ? ((u(), D)[l + 2 * d] = 1n, (u(), D)[l + 2 * d + 1] = m) : ((u(), D)[l + 2 * d] = 0n, (u(), ke)[l + 2 * d + 1] = m);
    }
    var v = $t(e, r, a, s, t);
    return Ze(o), v;
  };
  function er(e) {
    if (y) return N(0, 0, 1, e);
    b(`proc_exit: ${e}`), te = e, Fe() || (k.terminateAllThreads(), f.onExit?.(e), ue = !0), Er(e, new Rr(e));
  }
  function Br(e) {
    if (y) return N(1, 0, 0, e);
    b("exitOnMainThread"), rr(e);
  }
  var cn = (e, r) => {
    if (te = e, Qi(), y)
      throw c(!r), b(`Pthread ${W(Z())} called exit(${e}), posting exitOnMainThread.`), Br(e), "unwind";
    if (w(`main thread called exit(${e}): keepRuntimeAlive=${Fe()} (counter=${K})`), Fe() && !r) {
      var t = `program exited (with status: ${e}), but keepRuntimeAlive() is set (counter=${K}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
      Xe?.(t), w(t);
    }
    er(e);
  }, rr = cn, W = (e) => (c(typeof e == "number", `ptrToString expects a number, got ${typeof e}`), e >>>= 0, "0x" + e.toString(16).padStart(8, "0")), k = { unusedWorkers: [], runningWorkers: [], tlsInitFunctions: [], pthreads: {}, nextWorkerID: 1, init() {
    y || k.initMainThread();
  }, initMainThread() {
    for (var e = navigator.hardwareConcurrency; e--; )
      k.allocateUnusedWorker();
    xr(async () => {
      var r = k.loadWasmModuleToAllWorkers();
      Lr("loading-workers"), await r, Wr("loading-workers");
    });
  }, terminateAllThreads: () => {
    c(!y, "Internal Error! terminateAllThreads() can only ever be called from main application thread!"), b("terminateAllThreads");
    for (var e of k.runningWorkers)
      Dr(e);
    for (var e of k.unusedWorkers)
      Dr(e);
    k.unusedWorkers = [], k.runningWorkers = [], k.pthreads = {};
  }, returnWorkerToPool: (e) => {
    var r = e.pthread_ptr;
    delete k.pthreads[r], k.unusedWorkers.push(e), k.runningWorkers.splice(k.runningWorkers.indexOf(e), 1), e.pthread_ptr = 0, Bt(r);
  }, threadInitTLS() {
    b("threadInitTLS"), k.tlsInitFunctions.forEach((e) => e());
  }, loadWasmModuleToWorker: (e) => new Promise((r) => {
    e.onmessage = (o) => {
      var s = o.data, l = s.cmd;
      if (b(`main thread: received message '${l}' from worker. ${s}`), s.targetThread && s.targetThread != Z()) {
        var d = k.pthreads[s.targetThread];
        d ? d.postMessage(s, s.transferList) : w(`Internal error! Worker sent a message "${l}" to target pthread ${s.targetThread}, but that thread no longer exists!`);
        return;
      }
      l === "checkMailbox" ? Le() : l === "spawnThread" ? Ur(s) : l === "cleanupThread" ? pt(() => Mr(s.thread)) : l === "loaded" ? (e.loaded = !0, r(e)) : s.target === "setimmediate" ? e.postMessage(s) : l === "callHandler" ? f[s.handler](...s.args) : l && w(`worker sent an unknown command ${l}`);
    }, e.onerror = (o) => {
      var s = "worker sent an error!";
      throw e.pthread_ptr && (s = `Pthread ${W(e.pthread_ptr)} sent an error!`), w(`${s} ${o.filename}:${o.lineno}: ${o.message}`), o;
    }, c(j instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), c(oe instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
    var t = [], n = ["onExit", "onAbort", "print", "printErr"];
    for (var a of n)
      f.propertyIsEnumerable(a) && t.push(a);
    e.postMessage({ cmd: "load", handlers: t, wasmMemory: j, wasmModule: oe, workerID: e.workerID });
  }), async loadWasmModuleToAllWorkers() {
    return y ? void 0 : Promise.all(k.unusedWorkers.map(k.loadWasmModuleToWorker));
  }, allocateUnusedWorker() {
    var e;
    if (b(`Allocating a new web worker from ${import.meta.url}`), f.mainScriptUrlOrBlob) {
      var r = f.mainScriptUrlOrBlob;
      typeof r != "string" && (r = URL.createObjectURL(r)), e = new Worker(r, { type: "module", name: "em-pthread-" + k.nextWorkerID });
    } else e = new Worker(self.location.href, { type: "module", name: "em-pthread-" + k.nextWorkerID });
    e.workerID = k.nextWorkerID++, k.unusedWorkers.push(e);
  }, getNewWorker() {
    if (k.unusedWorkers.length == 0) {
      w("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.");
      return;
    }
    return k.unusedWorkers.pop();
  } }, zr = [], un = (e) => zr.push(e);
  function fn(e) {
    var r = (u(), h)[e + 52 >> 2], t = (u(), h)[e + 56 >> 2], n = r - t;
    b(`establishStackSpace: ${W(r)} -> ${W(n)}`), c(r != 0), c(n != 0), c(r > n, "stackHigh must be higher then stackLow"), Ht(r, n), Ze(r), br();
  }
  var tr = [], jr = (e) => {
    var r = tr[e];
    return r || (tr[e] = r = qt.get(e), G.isAsyncExport(r) && (tr[e] = r = G.makeAsyncFunction(r))), r;
  }, Hr = async (e, r) => {
    b(`invokeEntryPoint: ${W(e)}`), K = 0, nr = 0;
    var t = WebAssembly.promising(jr(e))(r);
    be();
    function n(a) {
      if (Fe()) {
        te = a;
        return;
      }
      hr(a);
    }
    t = await t, n(t);
  };
  Hr.isAsync = !0;
  var nr = !0, mn = (e) => k.tlsInitFunctions.push(e), ne = (e) => {
    ne.shown ||= {}, ne.shown[e] || (ne.shown[e] = 1, w(e));
  }, j, Vr = globalThis.TextDecoder && new TextDecoder(), Gr = (e, r, t, n) => {
    var a = r + t;
    if (n) return a;
    for (; e[r] && !(r >= a); ) ++r;
    return r;
  }, _e = (e, r = 0, t, n) => {
    var a = Gr(e, r, t, n);
    if (a - r > 16 && e.buffer && Vr)
      return Vr.decode(e.buffer instanceof ArrayBuffer ? e.subarray(r, a) : e.slice(r, a));
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
      var d = e[r++] & 63;
      if ((s & 240) == 224 ? s = (s & 15) << 12 | l << 6 | d : ((s & 248) != 240 && ne("Invalid UTF-8 leading byte " + W(s) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), s = (s & 7) << 18 | l << 12 | d << 6 | e[r++] & 63), s < 65536)
        o += String.fromCharCode(s);
      else {
        var m = s - 65536;
        o += String.fromCharCode(55296 | m >> 10, 56320 | m & 1023);
      }
    }
    return o;
  }, X = (e, r, t) => (c(typeof e == "number", `UTF8ToString expects a number (got ${typeof e})`), e ? _e((u(), $), e, r, t) : ""), _n = (e, r, t, n) => P(`Assertion failed: ${X(e)}, at: ` + [r ? X(r) : "unknown filename", t, n ? X(n) : "unknown function"]);
  class vn {
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
      r = r ? 1 : 0, (u(), O)[this.ptr + 12] = r;
    }
    get_caught() {
      return (u(), O)[this.ptr + 12] != 0;
    }
    set_rethrown(r) {
      r = r ? 1 : 0, (u(), O)[this.ptr + 13] = r;
    }
    get_rethrown() {
      return (u(), O)[this.ptr + 13] != 0;
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
  var hn = (e, r, t) => {
    var n = new vn(e);
    n.init(r, t), c(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
  };
  function qr(e, r, t, n) {
    return y ? N(2, 0, 1, e, r, t, n) : Kr(e, r, t, n);
  }
  var pn = () => !!globalThis.SharedArrayBuffer, Kr = (e, r, t, n) => {
    if (!pn())
      return b("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
    b("createThread: " + W(e));
    var a = [], o = 0;
    if (y && (a.length === 0 || o))
      return qr(e, r, t, n);
    var s = { startRoutine: t, pthread_ptr: e, arg: n, transferList: a };
    return y ? (s.cmd = "spawnThread", postMessage(s, a), 0) : Ur(s);
  }, Oe = () => {
    c(C.varargs != null);
    var e = (u(), I)[+C.varargs >> 2];
    return C.varargs += 4, e;
  }, ve = Oe, F = { isAbs: (e) => e.charAt(0) === "/", splitPath: (e) => {
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
    var r = F.isAbs(e), t = e.slice(-1) === "/";
    return e = F.normalizeArray(e.split("/").filter((n) => !!n), !r).join("/"), !e && !r && (e = "."), e && t && (e += "/"), (r ? "/" : "") + e;
  }, dirname: (e) => {
    var r = F.splitPath(e), t = r[0], n = r[1];
    return !t && !n ? "." : (n && (n = n.slice(0, -1)), t + n);
  }, basename: (e) => e && e.match(/([^\/]+|\/)\/*$/)[1], join: (...e) => F.normalize(e.join("/")), join2: (e, r) => F.normalize(e + "/" + r) }, gn = () => (e) => e.set(crypto.getRandomValues(new Uint8Array(e.byteLength))), ir = (e) => {
    (ir = gn())(e);
  }, he = { resolve: (...e) => {
    for (var r = "", t = !1, n = e.length - 1; n >= -1 && !t; n--) {
      var a = n >= 0 ? e[n] : i.cwd();
      if (typeof a != "string")
        throw new TypeError("Arguments to path.resolve must be strings");
      if (!a)
        return "";
      r = a + "/" + r, t = F.isAbs(a);
    }
    return r = F.normalizeArray(r.split("/").filter((o) => !!o), !t).join("/"), (t ? "/" : "") + r || ".";
  }, relative: (e, r) => {
    e = he.resolve(e).slice(1), r = he.resolve(r).slice(1);
    function t(m) {
      for (var v = 0; v < m.length && m[v] === ""; v++)
        ;
      for (var g = m.length - 1; g >= 0 && m[g] === ""; g--)
        ;
      return v > g ? [] : m.slice(v, g - v + 1);
    }
    for (var n = t(e.split("/")), a = t(r.split("/")), o = Math.min(n.length, a.length), s = o, l = 0; l < o; l++)
      if (n[l] !== a[l]) {
        s = l;
        break;
      }
    for (var d = [], l = s; l < n.length; l++)
      d.push("..");
    return d = d.concat(a.slice(s)), d.join("/");
  } }, ar = [], ie = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.charCodeAt(t);
      n <= 127 ? r++ : n <= 2047 ? r += 2 : n >= 55296 && n <= 57343 ? (r += 4, ++t) : r += 3;
    }
    return r;
  }, Xr = (e, r, t, n) => {
    if (c(typeof e == "string", `stringToUTF8Array expects a string (got ${typeof e})`), !(n > 0)) return 0;
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
        l > 1114111 && ne("Invalid Unicode code point " + W(l) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), r[t++] = 240 | l >> 18, r[t++] = 128 | l >> 12 & 63, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63, s++;
      }
    }
    return r[t] = 0, t - a;
  }, or = (e, r, t) => {
    var n = ie(e) + 1, a = new Array(n), o = Xr(e, a, 0, a.length);
    return a.length = o, a;
  }, yn = () => {
    if (!ar.length) {
      var e = null;
      if (globalThis.window?.prompt && (e = window.prompt("Input: "), e !== null && (e += `
`)), !e)
        return null;
      ar = or(e);
    }
    return ar.shift();
  }, ae = { ttys: [], init() {
  }, shutdown() {
  }, register(e, r) {
    ae.ttys[e] = { input: [], output: [], ops: r }, i.registerDevice(e, ae.stream_ops);
  }, stream_ops: { open(e) {
    var r = ae.ttys[e.node.rdev];
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
    return yn();
  }, put_char(e, r) {
    r === null || r === 10 ? (re(_e(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (re(_e(e.output)), e.output = []);
  }, ioctl_tcgets(e) {
    return { c_iflag: 25856, c_oflag: 5, c_cflag: 191, c_lflag: 35387, c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  }, ioctl_tcsets(e, r, t) {
    return 0;
  }, ioctl_tiocgwinsz(e) {
    return [24, 80];
  } }, default_tty1_ops: { put_char(e, r) {
    r === null || r === 10 ? (w(_e(e.output)), e.output = []) : r != 0 && e.output.push(r);
  }, fsync(e) {
    e.output?.length > 0 && (w(_e(e.output)), e.output = []);
  } } }, En = (e, r) => (u(), $).fill(0, e, e + r), Yr = (e, r) => (c(r, "alignment argument is required"), Math.ceil(e / r) * r), Jr = (e) => {
    e = Yr(e, 65536);
    var r = Lt(65536, e);
    return r && En(r, e), r;
  }, E = { ops_table: null, mount(e) {
    return E.createNode(null, "/", 16895, 0);
  }, createNode(e, r, t, n) {
    if (i.isBlkdev(t) || i.isFIFO(t))
      throw new i.ErrnoError(63);
    E.ops_table ||= { dir: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr, lookup: E.node_ops.lookup, mknod: E.node_ops.mknod, rename: E.node_ops.rename, unlink: E.node_ops.unlink, rmdir: E.node_ops.rmdir, readdir: E.node_ops.readdir, symlink: E.node_ops.symlink }, stream: { llseek: E.stream_ops.llseek } }, file: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr }, stream: { llseek: E.stream_ops.llseek, read: E.stream_ops.read, write: E.stream_ops.write, mmap: E.stream_ops.mmap, msync: E.stream_ops.msync } }, link: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr, readlink: E.node_ops.readlink }, stream: {} }, chrdev: { node: { getattr: E.node_ops.getattr, setattr: E.node_ops.setattr }, stream: i.chrdev_stream_ops } };
    var a = i.createNode(e, r, t, n);
    return i.isDir(a.mode) ? (a.node_ops = E.ops_table.dir.node, a.stream_ops = E.ops_table.dir.stream, a.contents = {}) : i.isFile(a.mode) ? (a.node_ops = E.ops_table.file.node, a.stream_ops = E.ops_table.file.stream, a.usedBytes = 0, a.contents = null) : i.isLink(a.mode) ? (a.node_ops = E.ops_table.link.node, a.stream_ops = E.ops_table.link.stream) : i.isChrdev(a.mode) && (a.node_ops = E.ops_table.chrdev.node, a.stream_ops = E.ops_table.chrdev.stream), a.atime = a.mtime = a.ctime = Date.now(), e && (e.contents[r] = a, e.atime = e.mtime = e.ctime = a.atime), a;
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
    var n = E.createNode(e, r, 41471, 0);
    return n.link = t, n;
  }, readlink(e) {
    if (!i.isLink(e.mode))
      throw new i.ErrnoError(28);
    return e.link;
  } }, stream_ops: { read(e, r, t, n, a) {
    var o = e.node.contents;
    if (a >= e.node.usedBytes) return 0;
    var s = Math.min(e.node.usedBytes - a, n);
    if (c(s >= 0), s > 8 && o.subarray)
      r.set(o.subarray(a, a + s), t);
    else
      for (var l = 0; l < s; l++) r[t + l] = o[a + l];
    return s;
  }, write(e, r, t, n, a, o) {
    if (c(!(r instanceof ArrayBuffer)), r.buffer === (u(), O).buffer && (o = !1), !n) return 0;
    var s = e.node;
    if (s.mtime = s.ctime = Date.now(), r.subarray && (!s.contents || s.contents.subarray)) {
      if (o)
        return c(a === 0, "canOwn must imply no weird position inside the file"), s.contents = r.subarray(t, t + n), s.usedBytes = n, n;
      if (s.usedBytes === 0 && a === 0)
        return s.contents = r.slice(t, t + n), s.usedBytes = n, n;
      if (a + n <= s.usedBytes)
        return s.contents.set(r.subarray(t, t + n), a), n;
    }
    if (E.expandFileStorage(s, a + n), s.contents.subarray && r.subarray)
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
    if (!(a & 2) && l && l.buffer === (u(), O).buffer)
      s = !1, o = l.byteOffset;
    else {
      if (s = !0, o = Jr(r), !o)
        throw new i.ErrnoError(48);
      l && ((t > 0 || t + r < l.length) && (l.subarray ? l = l.subarray(t, t + r) : l = Array.prototype.slice.call(l, t, t + r)), (u(), O).set(l, o));
    }
    return { ptr: o, allocated: s };
  }, msync(e, r, t, n, a) {
    return E.stream_ops.write(e, r, 0, n, t, !1), 0;
  } } }, wn = (e) => {
    var r = { r: 0, "r+": 2, w: 577, "w+": 578, a: 1089, "a+": 1090 }, t = r[e];
    if (typeof t > "u")
      throw new Error(`Unknown file open mode: ${e}`);
    return t;
  }, sr = (e, r) => {
    var t = 0;
    return e && (t |= 365), r && (t |= 146), t;
  }, bn = (e) => X(Wt(e)), Zr = { EPERM: 63, ENOENT: 44, ESRCH: 71, EINTR: 27, EIO: 29, ENXIO: 60, E2BIG: 1, ENOEXEC: 45, EBADF: 8, ECHILD: 12, EAGAIN: 6, EWOULDBLOCK: 6, ENOMEM: 48, EACCES: 2, EFAULT: 21, ENOTBLK: 105, EBUSY: 10, EEXIST: 20, EXDEV: 75, ENODEV: 43, ENOTDIR: 54, EISDIR: 31, EINVAL: 28, ENFILE: 41, EMFILE: 33, ENOTTY: 59, ETXTBSY: 74, EFBIG: 22, ENOSPC: 51, ESPIPE: 70, EROFS: 69, EMLINK: 34, EPIPE: 64, EDOM: 18, ERANGE: 68, ENOMSG: 49, EIDRM: 24, ECHRNG: 106, EL2NSYNC: 156, EL3HLT: 107, EL3RST: 108, ELNRNG: 109, EUNATCH: 110, ENOCSI: 111, EL2HLT: 112, EDEADLK: 16, ENOLCK: 46, EBADE: 113, EBADR: 114, EXFULL: 115, ENOANO: 104, EBADRQC: 103, EBADSLT: 102, EDEADLOCK: 16, EBFONT: 101, ENOSTR: 100, ENODATA: 116, ETIME: 117, ENOSR: 118, ENONET: 119, ENOPKG: 120, EREMOTE: 121, ENOLINK: 47, EADV: 122, ESRMNT: 123, ECOMM: 124, EPROTO: 65, EMULTIHOP: 36, EDOTDOT: 125, EBADMSG: 9, ENOTUNIQ: 126, EBADFD: 127, EREMCHG: 128, ELIBACC: 129, ELIBBAD: 130, ELIBSCN: 131, ELIBMAX: 132, ELIBEXEC: 133, ENOSYS: 52, ENOTEMPTY: 55, ENAMETOOLONG: 37, ELOOP: 32, EOPNOTSUPP: 138, EPFNOSUPPORT: 139, ECONNRESET: 15, ENOBUFS: 42, EAFNOSUPPORT: 5, EPROTOTYPE: 67, ENOTSOCK: 57, ENOPROTOOPT: 50, ESHUTDOWN: 140, ECONNREFUSED: 14, EADDRINUSE: 3, ECONNABORTED: 13, ENETUNREACH: 40, ENETDOWN: 38, ETIMEDOUT: 73, EHOSTDOWN: 142, EHOSTUNREACH: 23, EINPROGRESS: 26, EALREADY: 7, EDESTADDRREQ: 17, EMSGSIZE: 35, EPROTONOSUPPORT: 66, ESOCKTNOSUPPORT: 137, EADDRNOTAVAIL: 4, ENETRESET: 39, EISCONN: 30, ENOTCONN: 53, ETOOMANYREFS: 141, EUSERS: 136, EDQUOT: 19, ESTALE: 72, ENOTSUP: 138, ENOMEDIUM: 148, EILSEQ: 25, EOVERFLOW: 61, ECANCELED: 11, ENOTRECOVERABLE: 56, EOWNERDEAD: 62, ESTRPIPE: 135 }, Sn = async (e) => {
    var r = await qe(e);
    return c(r, `Loading data file "${e}" failed (no arrayBuffer).`), new Uint8Array(r);
  }, kn = (...e) => i.createDataFile(...e), Tn = (e) => {
    for (var r = e; ; ) {
      if (!me[e]) return e;
      e = r + Math.random();
    }
  }, Qr = [], Fn = async (e, r) => {
    typeof Browser < "u" && Browser.init();
    for (var t of Qr)
      if (t.canHandle(r))
        return c(t.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), t.handle(e, r);
    return e;
  }, et = async (e, r, t, n, a, o, s, l) => {
    var d = r ? he.resolve(F.join2(e, r)) : e, m = Tn(`cp ${d}`);
    Lr(m);
    try {
      var v = t;
      typeof t == "string" && (v = await Sn(t)), v = await Fn(v, d), l?.(), o || kn(e, r, v, n, a, s);
    } finally {
      Wr(m);
    }
  }, An = (e, r, t, n, a, o, s, l, d, m) => {
    et(e, r, t, n, a, l, d, m).then(o).catch(s);
  }, i = { root: null, mounts: [], devices: {}, streams: [], nextInode: 1, nameTable: null, currentPath: "/", initialized: !1, ignorePermissions: !0, filesystems: null, syncFSRequests: 0, readFiles: {}, ErrnoError: class extends Error {
    name = "ErrnoError";
    constructor(e) {
      super(fe ? bn(e) : ""), this.errno = e;
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
    r.follow_mount ??= !0, F.isAbs(e) || (e = i.cwd() + "/" + e);
    e: for (var t = 0; t < 40; t++) {
      for (var n = e.split("/").filter((m) => !!m), a = i.root, o = "/", s = 0; s < n.length; s++) {
        var l = s === n.length - 1;
        if (l && r.parent)
          break;
        if (n[s] !== ".") {
          if (n[s] === "..") {
            if (o = F.dirname(o), i.isRoot(a)) {
              e = o + "/" + n.slice(s + 1).join("/"), t--;
              continue e;
            } else
              a = a.parent;
            continue;
          }
          o = F.join2(o, n[s]);
          try {
            a = i.lookupNode(a, n[s]);
          } catch (m) {
            if (m?.errno === 44 && l && r.noent_okay)
              return { path: o };
            throw m;
          }
          if (i.isMountpoint(a) && (!l || r.follow_mount) && (a = a.mounted.root), i.isLink(a.mode) && (!l || r.follow)) {
            if (!a.node_ops.readlink)
              throw new i.ErrnoError(52);
            var d = a.node_ops.readlink(a);
            F.isAbs(d) || (d = F.dirname(o) + "/" + d), e = d + "/" + n.slice(s + 1).join("/");
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
    c(typeof e == "object");
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
    return c(r >= -1), e = Object.assign(new i.FSStream(), e), r == -1 && (r = i.nextfd()), e.fd = r, i.streams[r] = e, e;
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
    typeof e == "function" && (r = e, e = !1), i.syncFSRequests++, i.syncFSRequests > 1 && w(`warning: ${i.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
    var t = i.getMounts(i.root.mount), n = 0;
    function a(l) {
      return c(i.syncFSRequests > 0), i.syncFSRequests--, r(l);
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
    var l = { type: e, opts: r, mountpoint: t, mounts: [] }, d = e.mount(l);
    return d.mount = l, l.root = d, n ? i.root = d : o && (o.mounted = l, o.mount && o.mount.mounts.push(l)), d;
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
    var d = t.mount.mounts.indexOf(n);
    c(d !== -1), t.mount.mounts.splice(d, 1);
  }, lookup(e, r) {
    return e.node_ops.lookup(e, r);
  }, mknod(e, r, t) {
    var n = i.lookupPath(e, { parent: !0 }), a = n.node, o = F.basename(e);
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
        (n || F.isAbs(e)) && (n += "/"), n += a;
        try {
          i.mkdir(n, r);
        } catch (o) {
          if (o.errno != 20) throw o;
        }
      }
  }, mkdev(e, r, t) {
    return typeof t > "u" && (t = r, r = 438), r |= 8192, i.mknod(e, r, t);
  }, symlink(e, r) {
    if (!he.resolve(e))
      throw new i.ErrnoError(44);
    var t = i.lookupPath(r, { parent: !0 }), n = t.node;
    if (!n)
      throw new i.ErrnoError(44);
    var a = F.basename(r), o = i.mayCreate(n, a);
    if (o)
      throw new i.ErrnoError(o);
    if (!n.node_ops.symlink)
      throw new i.ErrnoError(63);
    return n.node_ops.symlink(n, a, e);
  }, rename(e, r) {
    var t = F.dirname(e), n = F.dirname(r), a = F.basename(e), o = F.basename(r), s, l, d;
    if (s = i.lookupPath(e, { parent: !0 }), l = s.node, s = i.lookupPath(r, { parent: !0 }), d = s.node, !l || !d) throw new i.ErrnoError(44);
    if (l.mount !== d.mount)
      throw new i.ErrnoError(75);
    var m = i.lookupNode(l, a), v = he.relative(e, n);
    if (v.charAt(0) !== ".")
      throw new i.ErrnoError(28);
    if (v = he.relative(r, t), v.charAt(0) !== ".")
      throw new i.ErrnoError(55);
    var g;
    try {
      g = i.lookupNode(d, o);
    } catch {
    }
    if (m !== g) {
      var _ = i.isDir(m.mode), p = i.mayDelete(l, a, _);
      if (p)
        throw new i.ErrnoError(p);
      if (p = g ? i.mayDelete(d, o, _) : i.mayCreate(d, o), p)
        throw new i.ErrnoError(p);
      if (!l.node_ops.rename)
        throw new i.ErrnoError(63);
      if (i.isMountpoint(m) || g && i.isMountpoint(g))
        throw new i.ErrnoError(10);
      if (d !== l && (p = i.nodePermissions(l, "w"), p))
        throw new i.ErrnoError(p);
      i.hashRemoveNode(m);
      try {
        l.node_ops.rename(m, d, o), m.parent = d;
      } catch (T) {
        throw T;
      } finally {
        i.hashAddNode(m);
      }
    }
  }, rmdir(e) {
    var r = i.lookupPath(e, { parent: !0 }), t = r.node, n = F.basename(e), a = i.lookupNode(t, n), o = i.mayDelete(t, n, !0);
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
    var n = F.basename(e), a = i.lookupNode(t, n), o = i.mayDelete(t, n, !1);
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
    r = typeof r == "string" ? wn(r) : r, r & 64 ? t = t & 4095 | 32768 : t = 0;
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
  }, read(e, r, t, n, a) {
    if (c(t >= 0), n < 0 || a < 0)
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
    if (c(t >= 0), n < 0 || a < 0)
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
    return c(t >= 0), e.stream_ops.msync ? e.stream_ops.msync(e, r, t, n, a) : 0;
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
    typeof r == "string" && (r = new Uint8Array(or(r))), ArrayBuffer.isView(r) ? i.write(n, r, 0, r.byteLength, void 0, t.canOwn) : P("Unsupported data type"), i.close(n);
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
    i.mkdir("/dev"), i.registerDevice(i.makedev(1, 3), { read: () => 0, write: (n, a, o, s, l) => s, llseek: () => 0 }), i.mkdev("/dev/null", i.makedev(1, 3)), ae.register(i.makedev(5, 0), ae.default_tty_ops), ae.register(i.makedev(6, 0), ae.default_tty1_ops), i.mkdev("/dev/tty", i.makedev(5, 0)), i.mkdev("/dev/tty1", i.makedev(6, 0));
    var e = new Uint8Array(1024), r = 0, t = () => (r === 0 && (ir(e), r = e.byteLength), e[--r]);
    i.createDevice("/dev", "random", t), i.createDevice("/dev", "urandom", t), i.mkdir("/dev/shm"), i.mkdir("/dev/shm/tmp");
  }, createSpecialDirectories() {
    i.mkdir("/proc");
    var e = i.mkdir("/proc/self");
    i.mkdir("/proc/self/fd"), i.mount({ mount() {
      var r = i.createNode(e, "fd", 16895, 73);
      return r.stream_ops = { llseek: E.stream_ops.llseek }, r.node_ops = { lookup(t, n) {
        var a = +n, o = i.getStreamChecked(a), s = { parent: null, mount: { mountpoint: "fake" }, node_ops: { readlink: () => o.path }, id: a + 1 };
        return s.parent = s, s;
      }, readdir() {
        return Array.from(i.streams.entries()).filter(([t, n]) => n).map(([t, n]) => t.toString());
      } }, r;
    } }, {}, "/proc/self/fd");
  }, createStandardStreams(e, r, t) {
    e ? i.createDevice("/dev", "stdin", e) : i.symlink("/dev/tty", "/dev/stdin"), r ? i.createDevice("/dev", "stdout", null, r) : i.symlink("/dev/tty", "/dev/stdout"), t ? i.createDevice("/dev", "stderr", null, t) : i.symlink("/dev/tty1", "/dev/stderr");
    var n = i.open("/dev/stdin", 0), a = i.open("/dev/stdout", 1), o = i.open("/dev/stderr", 1);
    c(n.fd === 0, `invalid handle for stdin (${n.fd})`), c(a.fd === 1, `invalid handle for stdout (${a.fd})`), c(o.fd === 2, `invalid handle for stderr (${o.fd})`);
  }, staticInit() {
    i.nameTable = new Array(4096), i.mount(E, {}, "/"), i.createDefaultDirectories(), i.createDefaultDevices(), i.createSpecialDirectories(), i.filesystems = { MEMFS: E };
  }, init(e, r, t) {
    c(!i.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), i.initialized = !0, e ??= f.stdin, r ??= f.stdout, t ??= f.stderr, i.createStandardStreams(e, r, t);
  }, quit() {
    i.initialized = !1, mr(0);
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
      n.parentExists = !0, n.parentPath = t.path, n.parentObject = t.node, n.name = F.basename(e), t = i.lookupPath(e, { follow: !r }), n.exists = !0, n.path = t.path, n.object = t.node, n.name = t.node.name, n.isRoot = t.path === "/";
    } catch (a) {
      n.error = a.errno;
    }
    return n;
  }, createPath(e, r, t, n) {
    e = typeof e == "string" ? e : i.getPath(e);
    for (var a = r.split("/").reverse(); a.length; ) {
      var o = a.pop();
      if (o) {
        var s = F.join2(e, o);
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
    var o = F.join2(typeof e == "string" ? e : i.getPath(e), r), s = sr(n, a);
    return i.create(o, s);
  }, createDataFile(e, r, t, n, a, o) {
    var s = r;
    e && (e = typeof e == "string" ? e : i.getPath(e), s = r ? F.join2(e, r) : e);
    var l = sr(n, a), d = i.create(s, l);
    if (t) {
      if (typeof t == "string") {
        for (var m = new Array(t.length), v = 0, g = t.length; v < g; ++v) m[v] = t.charCodeAt(v);
        t = m;
      }
      i.chmod(d, l | 146);
      var _ = i.open(d, 577);
      i.write(_, t, 0, t.length, 0, o), i.close(_), i.chmod(d, l);
    }
  }, createDevice(e, r, t, n) {
    var a = F.join2(typeof e == "string" ? e : i.getPath(e), r), o = sr(!!t, !!n);
    i.createDevice.major ??= 64;
    var s = i.makedev(i.createDevice.major++, 0);
    return i.registerDevice(s, { open(l) {
      l.seekable = !1;
    }, close(l) {
      n?.buffer?.length && n(10);
    }, read(l, d, m, v, g) {
      for (var _ = 0, p = 0; p < v; p++) {
        var T;
        try {
          T = t();
        } catch {
          throw new i.ErrnoError(29);
        }
        if (T === void 0 && _ === 0)
          throw new i.ErrnoError(6);
        if (T == null) break;
        _++, d[m + p] = T;
      }
      return _ && (l.node.atime = Date.now()), _;
    }, write(l, d, m, v, g) {
      for (var _ = 0; _ < v; _++)
        try {
          n(d[m + _]);
        } catch {
          throw new i.ErrnoError(29);
        }
      return v && (l.node.mtime = l.node.ctime = Date.now()), _;
    } }), i.mkdev(a, o, s);
  }, forceLoadFile(e) {
    if (e.isDevice || e.isFolder || e.link || e.contents) return !0;
    if (globalThis.XMLHttpRequest)
      P("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    else
      try {
        e.contents = De(e.url);
      } catch {
        throw new i.ErrnoError(29);
      }
  }, createLazyFile(e, r, t, n, a) {
    class o {
      lengthKnown = !1;
      chunks = [];
      get(_) {
        if (!(_ > this.length - 1 || _ < 0)) {
          var p = _ % this.chunkSize, T = _ / this.chunkSize | 0;
          return this.getter(T)[p];
        }
      }
      setDataGetter(_) {
        this.getter = _;
      }
      cacheLength() {
        var _ = new XMLHttpRequest();
        _.open("HEAD", t, !1), _.send(null), _.status >= 200 && _.status < 300 || _.status === 304 || P("Couldn't load " + t + ". Status: " + _.status);
        var p = Number(_.getResponseHeader("Content-length")), T, S = (T = _.getResponseHeader("Accept-Ranges")) && T === "bytes", R = (T = _.getResponseHeader("Content-Encoding")) && T === "gzip", L = 1024 * 1024;
        S || (L = p);
        var U = (q, Ee) => {
          q > Ee && P("invalid range (" + q + ", " + Ee + ") or no bytes requested!"), Ee > p - 1 && P("only " + p + " bytes available! programmer error!");
          var x = new XMLHttpRequest();
          return x.open("GET", t, !1), p !== L && x.setRequestHeader("Range", "bytes=" + q + "-" + Ee), x.responseType = "arraybuffer", x.overrideMimeType && x.overrideMimeType("text/plain; charset=x-user-defined"), x.send(null), x.status >= 200 && x.status < 300 || x.status === 304 || P("Couldn't load " + t + ". Status: " + x.status), x.response !== void 0 ? new Uint8Array(x.response || []) : or(x.responseText || "");
        }, Ce = this;
        Ce.setDataGetter((q) => {
          var Ee = q * L, x = (q + 1) * L - 1;
          return x = Math.min(x, p - 1), typeof Ce.chunks[q] > "u" && (Ce.chunks[q] = U(Ee, x)), typeof Ce.chunks[q] > "u" && P("doXHR failed!"), Ce.chunks[q];
        }), (R || !p) && (L = p = 1, p = this.getter(0).length, L = p, re("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = p, this._chunkSize = L, this.lengthKnown = !0;
      }
      get length() {
        return this.lengthKnown || this.cacheLength(), this._length;
      }
      get chunkSize() {
        return this.lengthKnown || this.cacheLength(), this._chunkSize;
      }
    }
    if (globalThis.XMLHttpRequest) {
      ee || P("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
      var s = new o(), l = { isDevice: !1, contents: s };
    } else
      var l = { isDevice: !1, url: t };
    var d = i.createFile(e, r, l, n, a);
    l.contents ? d.contents = l.contents : l.url && (d.contents = null, d.url = l.url), Object.defineProperties(d, { usedBytes: { get: function() {
      return this.contents.length;
    } } });
    var m = {};
    for (const [g, _] of Object.entries(d.stream_ops))
      m[g] = (...p) => (i.forceLoadFile(d), _(...p));
    function v(g, _, p, T, S) {
      var R = g.node.contents;
      if (S >= R.length) return 0;
      var L = Math.min(R.length - S, T);
      if (c(L >= 0), R.slice)
        for (var U = 0; U < L; U++)
          _[p + U] = R[S + U];
      else
        for (var U = 0; U < L; U++)
          _[p + U] = R.get(S + U);
      return L;
    }
    return m.read = (g, _, p, T, S) => (i.forceLoadFile(d), v(g, _, p, T, S)), m.mmap = (g, _, p, T, S) => {
      i.forceLoadFile(d);
      var R = Jr(_);
      if (!R)
        throw new i.ErrnoError(48);
      return v(g, (u(), O), R, _, p), { ptr: R, allocated: !0 };
    }, d.stream_ops = m, d;
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
    if (F.isAbs(r))
      return r;
    var n;
    if (e === -100)
      n = i.cwd();
    else {
      var a = C.getStreamFromFD(e);
      n = a.path;
    }
    if (r.length == 0) {
      if (!t)
        throw new i.ErrnoError(44);
      return n;
    }
    return n + "/" + r;
  }, writeStat(e, r) {
    (u(), h)[e >> 2] = r.dev, (u(), h)[e + 4 >> 2] = r.mode, (u(), h)[e + 8 >> 2] = r.nlink, (u(), h)[e + 12 >> 2] = r.uid, (u(), h)[e + 16 >> 2] = r.gid, (u(), h)[e + 20 >> 2] = r.rdev, (u(), D)[e + 24 >> 3] = BigInt(r.size), (u(), I)[e + 32 >> 2] = 4096, (u(), I)[e + 36 >> 2] = r.blocks;
    var t = r.atime.getTime(), n = r.mtime.getTime(), a = r.ctime.getTime();
    return (u(), D)[e + 40 >> 3] = BigInt(Math.floor(t / 1e3)), (u(), h)[e + 48 >> 2] = t % 1e3 * 1e3 * 1e3, (u(), D)[e + 56 >> 3] = BigInt(Math.floor(n / 1e3)), (u(), h)[e + 64 >> 2] = n % 1e3 * 1e3 * 1e3, (u(), D)[e + 72 >> 3] = BigInt(Math.floor(a / 1e3)), (u(), h)[e + 80 >> 2] = a % 1e3 * 1e3 * 1e3, (u(), D)[e + 88 >> 3] = BigInt(r.ino), 0;
  }, writeStatFs(e, r) {
    (u(), h)[e + 4 >> 2] = r.bsize, (u(), h)[e + 60 >> 2] = r.bsize, (u(), D)[e + 8 >> 3] = BigInt(r.blocks), (u(), D)[e + 16 >> 3] = BigInt(r.bfree), (u(), D)[e + 24 >> 3] = BigInt(r.bavail), (u(), D)[e + 32 >> 3] = BigInt(r.files), (u(), D)[e + 40 >> 3] = BigInt(r.ffree), (u(), h)[e + 48 >> 2] = r.fsid, (u(), h)[e + 64 >> 2] = r.flags, (u(), h)[e + 56 >> 2] = r.namelen;
  }, doMsync(e, r, t, n, a) {
    if (!i.isFile(r.node.mode))
      throw new i.ErrnoError(43);
    if (n & 2)
      return 0;
    var o = (u(), $).slice(e, e + t);
    i.msync(r, o, a, t, n);
  }, getStreamFromFD(e) {
    var r = i.getStreamChecked(e);
    return r;
  }, varargs: void 0, getStr(e) {
    var r = X(e);
    return r;
  } };
  function rt(e, r, t) {
    if (y) return N(3, 0, 1, e, r, t);
    C.varargs = t;
    try {
      var n = C.getStreamFromFD(e);
      switch (r) {
        case 0: {
          var a = Oe();
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
          var a = Oe();
          return n.flags |= a, 0;
        }
        case 12: {
          var a = ve(), s = 0;
          return (u(), se)[a + s >> 1] = 2, 0;
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
    if (y) return N(4, 0, 1, e, r);
    try {
      return C.writeStat(r, i.fstat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function nt(e, r, t) {
    if (y) return N(5, 0, 1, e, r, t);
    C.varargs = t;
    try {
      var n = C.getStreamFromFD(e);
      switch (r) {
        case 21509:
          return n.tty ? 0 : -59;
        case 21505: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tcgets) {
            var a = n.tty.ops.ioctl_tcgets(n), o = ve();
            (u(), I)[o >> 2] = a.c_iflag || 0, (u(), I)[o + 4 >> 2] = a.c_oflag || 0, (u(), I)[o + 8 >> 2] = a.c_cflag || 0, (u(), I)[o + 12 >> 2] = a.c_lflag || 0;
            for (var s = 0; s < 32; s++)
              (u(), O)[o + s + 17] = a.c_cc[s] || 0;
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
            for (var o = ve(), l = (u(), I)[o >> 2], d = (u(), I)[o + 4 >> 2], m = (u(), I)[o + 8 >> 2], v = (u(), I)[o + 12 >> 2], g = [], s = 0; s < 32; s++)
              g.push((u(), O)[o + s + 17]);
            return n.tty.ops.ioctl_tcsets(n.tty, r, { c_iflag: l, c_oflag: d, c_cflag: m, c_lflag: v, c_cc: g });
          }
          return 0;
        }
        case 21519: {
          if (!n.tty) return -59;
          var o = ve();
          return (u(), I)[o >> 2] = 0, 0;
        }
        case 21520:
          return n.tty ? -28 : -59;
        case 21537:
        case 21531: {
          var o = ve();
          return i.ioctl(n, r, o);
        }
        case 21523: {
          if (!n.tty) return -59;
          if (n.tty.ops.ioctl_tiocgwinsz) {
            var _ = n.tty.ops.ioctl_tiocgwinsz(n.tty), o = ve();
            (u(), se)[o >> 1] = _[0], (u(), se)[o + 2 >> 1] = _[1];
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
    if (y) return N(6, 0, 1, e, r);
    try {
      return e = C.getStr(e), C.writeStat(r, i.lstat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function at(e, r, t, n) {
    if (y) return N(7, 0, 1, e, r, t, n);
    try {
      r = C.getStr(r);
      var a = n & 256, o = n & 4096;
      return n = n & -6401, c(!n, `unknown flags in __syscall_newfstatat: ${n}`), r = C.calculateAt(e, r, o), C.writeStat(t, a ? i.lstat(r) : i.stat(r));
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return -s.errno;
    }
  }
  function ot(e, r, t, n) {
    if (y) return N(8, 0, 1, e, r, t, n);
    C.varargs = n;
    try {
      r = C.getStr(r), r = C.calculateAt(e, r);
      var a = n ? Oe() : 0;
      return i.open(r, t, a).fd;
    } catch (o) {
      if (typeof i > "u" || o.name !== "ErrnoError") throw o;
      return -o.errno;
    }
  }
  function st(e, r) {
    if (y) return N(9, 0, 1, e, r);
    try {
      return e = C.getStr(e), C.writeStat(r, i.stat(e));
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  var Pn = () => P("native code called abort()"), B = (e) => {
    for (var r = ""; ; ) {
      var t = (u(), $)[e++];
      if (!t) return r;
      r += String.fromCharCode(t);
    }
  }, pe = {}, ge = {}, xe = {}, Cn = class extends Error {
    constructor(r) {
      super(r), this.name = "BindingError";
    }
  }, z = (e) => {
    throw new Cn(e);
  };
  function In(e, r, t = {}) {
    var n = r.name;
    if (e || z(`type "${n}" must have a positive integer typeid pointer`), ge.hasOwnProperty(e)) {
      if (t.ignoreDuplicateRegistrations)
        return;
      z(`Cannot register type '${n}' twice`);
    }
    if (ge[e] = r, delete xe[e], pe.hasOwnProperty(e)) {
      var a = pe[e];
      delete pe[e], a.forEach((o) => o());
    }
  }
  function H(e, r, t = {}) {
    return In(e, r, t);
  }
  var lt = (e, r, t) => {
    switch (r) {
      case 1:
        return t ? (n) => (u(), O)[n] : (n) => (u(), $)[n];
      case 2:
        return t ? (n) => (u(), se)[n >> 1] : (n) => (u(), Se)[n >> 1];
      case 4:
        return t ? (n) => (u(), I)[n >> 2] : (n) => (u(), h)[n >> 2];
      case 8:
        return t ? (n) => (u(), D)[n >> 3] : (n) => (u(), Ar)[n >> 3];
      default:
        throw new TypeError(`invalid integer width (${r}): ${e}`);
    }
  }, We = (e) => {
    if (e === null)
      return "null";
    var r = typeof e;
    return r === "object" || r === "array" || r === "function" ? e.toString() : "" + e;
  }, dt = (e, r, t, n) => {
    if (r < t || r > n)
      throw new TypeError(`Passing a number "${We(r)}" from JS side to C/C++ side to an argument of type "${e}", which is outside the valid range [${t}, ${n}]!`);
  }, Rn = (e, r, t, n, a) => {
    r = B(r);
    const o = n === 0n;
    let s = (l) => l;
    if (o) {
      const l = t * 8;
      s = (d) => BigInt.asUintN(l, d), a = s(a);
    }
    H(e, { name: r, fromWireType: s, toWireType: (l, d) => {
      if (typeof d == "number")
        d = BigInt(d);
      else if (typeof d != "bigint")
        throw new TypeError(`Cannot convert "${We(d)}" to ${this.name}`);
      return dt(r, d, n, a), d;
    }, readValueFromPointer: lt(r, t, !o), destructorFunction: null });
  }, Dn = (e, r, t, n) => {
    r = B(r), H(e, { name: r, fromWireType: function(a) {
      return !!a;
    }, toWireType: function(a, o) {
      return o ? t : n;
    }, readValueFromPointer: function(a) {
      return this.fromWireType((u(), $)[a]);
    }, destructorFunction: null });
  }, ct = [], Y = [0, 1, , 1, null, 1, !0, 1, !1, 1], Mn = (e) => {
    e > 9 && --Y[e + 1] === 0 && (c(Y[e] !== void 0, "Decref for unallocated handle."), Y[e] = void 0, ct.push(e));
  }, ut = { toValue: (e) => (e || z(`Cannot use deleted val. handle = ${e}`), c(e === 2 || Y[e] !== void 0 && e % 2 === 0, `invalid handle: ${e}`), Y[e]), toHandle: (e) => {
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
        const r = ct.pop() || Y.length;
        return Y[r] = e, Y[r + 1] = 1, r;
      }
    }
  } };
  function lr(e) {
    return this.fromWireType((u(), h)[e >> 2]);
  }
  var Nn = { name: "emscripten::val", fromWireType: (e) => {
    var r = ut.toValue(e);
    return Mn(e), r;
  }, toWireType: (e, r) => ut.toHandle(r), readValueFromPointer: lr, destructorFunction: null }, On = (e) => H(e, Nn), xn = (e, r) => {
    switch (r) {
      case 4:
        return function(t) {
          return this.fromWireType((u(), Fr)[t >> 2]);
        };
      case 8:
        return function(t) {
          return this.fromWireType((u(), ke)[t >> 3]);
        };
      default:
        throw new TypeError(`invalid float width (${r}): ${e}`);
    }
  }, Wn = (e, r, t) => {
    r = B(r), H(e, { name: r, fromWireType: (n) => n, toWireType: (n, a) => {
      if (typeof a != "number" && typeof a != "boolean")
        throw new TypeError(`Cannot convert ${We(a)} to ${this.name}`);
      return a;
    }, readValueFromPointer: xn(r, t), destructorFunction: null });
  }, ft = (e, r) => Object.defineProperty(r, "name", { value: e }), Ln = (e) => {
    for (; e.length; ) {
      var r = e.pop(), t = e.pop();
      t(r);
    }
  };
  function mt(e) {
    for (var r = 1; r < e.length; ++r)
      if (e[r] !== null && e[r].destructorFunction === void 0)
        return !0;
    return !1;
  }
  function Un(e, r, t, n, a) {
    if (e < r || e > t) {
      var o = r == t ? r : `${r} to ${t}`;
      a(`function ${n} called with ${e} arguments, expected ${o}`);
    }
  }
  function $n(e, r, t, n) {
    for (var a = mt(e), o = e.length - 2, s = [], l = ["fn"], d = 0; d < o; ++d)
      s.push(`arg${d}`), l.push(`arg${d}Wired`);
    s = s.join(","), l = l.join(",");
    var m = `return function (${s}) {
`;
    m += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, a && (m += `var destructors = [];
`);
    for (var v = a ? "destructors" : "null", g = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "fromRetWire", "toClassParamWire"], d = 0; d < o; ++d) {
      var _ = `toArg${d}Wire`;
      m += `var arg${d}Wired = ${_}(${v}, arg${d});
`, g.push(_);
    }
    m += (t || n ? "var rv = " : "") + `invoker(${l});
`;
    var p = t ? "rv" : "";
    if (m += `function onDone(${p}) {
`, a)
      m += `runDestructors(destructors);
`;
    else
      for (var d = 2; d < e.length; ++d) {
        var T = d === 1 ? "thisWired" : "arg" + (d - 2) + "Wired";
        e[d].destructorFunction !== null && (m += `${T}_dtor(${T});
`, g.push(`${T}_dtor`));
      }
    return t && (m += `var ret = fromRetWire(rv);
return ret;
`), m += `}
`, m += "return " + (n ? "rv.then(onDone)" : `onDone(${p})`) + ";", m += `}
`, g.push("checkArgCount", "minArgs", "maxArgs"), m = `if (arguments.length !== ${g.length}){ throw new Error(humanName + "Expected ${g.length} closure arguments " + arguments.length + " given."); }
${m}`, new Function(g, m);
  }
  function Bn(e) {
    for (var r = e.length - 2, t = e.length - 1; t >= 2 && e[t].optional; --t)
      r--;
    return r;
  }
  function zn(e, r, t, n, a, o) {
    var s = r.length;
    s < 2 && z("argTypes array size mismatch! Must at least get return value and 'this' types!");
    for (var l = r[1] !== null && t !== null, d = mt(r), m = !r[0].isVoid, v = s - 2, g = Bn(r), _ = r[0], p = r[1], T = [e, z, n, a, Ln, _.fromWireType.bind(_), p?.toWireType.bind(p)], S = 2; S < s; ++S) {
      var R = r[S];
      T.push(R.toWireType.bind(R));
    }
    if (!d)
      for (var S = 2; S < r.length; ++S)
        r[S].destructorFunction !== null && T.push(r[S].destructorFunction);
    T.push(Un, g, v);
    var U = $n(r, l, m, o)(...T);
    return ft(e, U);
  }
  var jn = (e, r, t) => {
    if (e[r].overloadTable === void 0) {
      var n = e[r];
      e[r] = function(...a) {
        return e[r].overloadTable.hasOwnProperty(a.length) || z(`Function '${t}' called with an invalid number of arguments (${a.length}) - expects one of (${e[r].overloadTable})!`), e[r].overloadTable[a.length].apply(this, a);
      }, e[r].overloadTable = [], e[r].overloadTable[n.argCount] = n;
    }
  }, Hn = (e, r, t) => {
    f.hasOwnProperty(e) ? ((t === void 0 || f[e].overloadTable !== void 0 && f[e].overloadTable[t] !== void 0) && z(`Cannot register public name '${e}' twice`), jn(f, e, e), f[e].overloadTable.hasOwnProperty(t) && z(`Cannot register multiple overloads of a function with the same number of arguments (${t})!`), f[e].overloadTable[t] = r) : (f[e] = r, f[e].argCount = t);
  }, Vn = (e, r) => {
    for (var t = [], n = 0; n < e; n++)
      t.push((u(), h)[r + n * 4 >> 2]);
    return t;
  }, Gn = class extends Error {
    constructor(r) {
      super(r), this.name = "InternalError";
    }
  }, _t = (e) => {
    throw new Gn(e);
  }, qn = (e, r, t) => {
    f.hasOwnProperty(e) || _t("Replacing nonexistent public symbol"), f[e].overloadTable !== void 0 && t !== void 0 ? f[e].overloadTable[t] = r : (f[e] = r, f[e].argCount = t);
  }, Kn = (e, r, t = !1) => {
    e = B(e);
    function n() {
      var o = jr(r);
      return t && (o = WebAssembly.promising(o)), o;
    }
    var a = n();
    return typeof a != "function" && z(`unknown function pointer with signature ${e}: ${r}`), a;
  };
  class Xn extends Error {
  }
  var Yn = (e) => {
    var r = Ot(e), t = B(r);
    return J(r), t;
  }, Jn = (e, r) => {
    var t = [], n = {};
    function a(o) {
      if (!n[o] && !ge[o]) {
        if (xe[o]) {
          xe[o].forEach(a);
          return;
        }
        t.push(o), n[o] = !0;
      }
    }
    throw r.forEach(a), new Xn(`${e}: ` + t.map(Yn).join([", "]));
  }, Zn = (e, r, t) => {
    e.forEach((l) => xe[l] = r);
    function n(l) {
      var d = t(l);
      d.length !== e.length && _t("Mismatched type converter count");
      for (var m = 0; m < e.length; ++m)
        H(e[m], d[m]);
    }
    var a = new Array(r.length), o = [], s = 0;
    for (let [l, d] of r.entries())
      ge.hasOwnProperty(d) ? a[l] = ge[d] : (o.push(d), pe.hasOwnProperty(d) || (pe[d] = []), pe[d].push(() => {
        a[l] = ge[d], ++s, s === o.length && n(a);
      }));
    o.length === 0 && n(a);
  }, Qn = (e) => {
    e = e.trim();
    const r = e.indexOf("(");
    return r === -1 ? e : (c(e.endsWith(")"), "Parentheses for argument names should match."), e.slice(0, r));
  }, ei = (e, r, t, n, a, o, s, l) => {
    var d = Vn(r, t);
    e = B(e), e = Qn(e), a = Kn(n, a, s), Hn(e, function() {
      Jn(`Cannot call ${e} due to unbound types`, d);
    }, r - 1), Zn([], d, (m) => {
      var v = [m[0], null].concat(m.slice(1));
      return qn(e, zn(e, v, null, a, o, s), r - 1), [];
    });
  }, ri = (e, r, t, n, a) => {
    r = B(r);
    const o = n === 0;
    let s = (d) => d;
    if (o) {
      var l = 32 - 8 * t;
      s = (d) => d << l >>> l, a = s(a);
    }
    H(e, { name: r, fromWireType: s, toWireType: (d, m) => {
      if (typeof m != "number" && typeof m != "boolean")
        throw new TypeError(`Cannot convert "${We(m)}" to ${r}`);
      return dt(r, m, n, a), m;
    }, readValueFromPointer: lt(r, t, n !== 0), destructorFunction: null });
  }, ti = (e, r, t) => {
    var n = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array], a = n[r];
    function o(s) {
      var l = (u(), h)[s >> 2], d = (u(), h)[s + 4 >> 2];
      return new a((u(), O).buffer, d, l);
    }
    t = B(t), H(e, { name: t, fromWireType: o, readValueFromPointer: o }, { ignoreDuplicateRegistrations: !0 });
  }, V = (e, r, t) => (c(typeof t == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), Xr(e, (u(), $), r, t)), ni = (e, r) => {
    r = B(r), H(e, { name: r, fromWireType(t) {
      var n = (u(), h)[t >> 2], a = t + 4, o;
      return o = X(a, n, !0), J(t), o;
    }, toWireType(t, n) {
      n instanceof ArrayBuffer && (n = new Uint8Array(n));
      var a, o = typeof n == "string";
      o || ArrayBuffer.isView(n) && n.BYTES_PER_ELEMENT == 1 || z("Cannot pass non-string to std::string"), o ? a = ie(n) : a = n.length;
      var s = Pe(4 + a + 1), l = s + 4;
      return (u(), h)[s >> 2] = a, o ? V(n, l, a + 1) : (u(), $).set(n, l), t !== null && t.push(J, s), s;
    }, readValueFromPointer: lr, destructorFunction(t) {
      J(t);
    } });
  }, vt = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, ii = (e, r, t) => {
    c(e % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
    var n = e >> 1, a = Gr((u(), Se), n, r / 2, t);
    if (a - n > 16 && vt) return vt.decode((u(), Se).slice(n, a));
    for (var o = "", s = n; s < a; ++s) {
      var l = (u(), Se)[s];
      o += String.fromCharCode(l);
    }
    return o;
  }, ai = (e, r, t) => {
    if (c(r % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), c(typeof t == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 2) return 0;
    t -= 2;
    for (var n = r, a = t < e.length * 2 ? t / 2 : e.length, o = 0; o < a; ++o) {
      var s = e.charCodeAt(o);
      (u(), se)[r >> 1] = s, r += 2;
    }
    return (u(), se)[r >> 1] = 0, r - n;
  }, oi = (e) => e.length * 2, si = (e, r, t) => {
    c(e % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
    for (var n = "", a = e >> 2, o = 0; !(o >= r / 4); o++) {
      var s = (u(), h)[a + o];
      if (!s && !t) break;
      n += String.fromCodePoint(s);
    }
    return n;
  }, li = (e, r, t) => {
    if (c(r % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), c(typeof t == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 4) return 0;
    for (var n = r, a = n + t - 4, o = 0; o < e.length; ++o) {
      var s = e.codePointAt(o);
      if (s > 65535 && o++, (u(), I)[r >> 2] = s, r += 4, r + 4 > a) break;
    }
    return (u(), I)[r >> 2] = 0, r - n;
  }, di = (e) => {
    for (var r = 0, t = 0; t < e.length; ++t) {
      var n = e.codePointAt(t);
      n > 65535 && t++, r += 4;
    }
    return r;
  }, ci = (e, r, t) => {
    t = B(t);
    var n, a, o;
    r === 2 ? (n = ii, a = ai, o = oi) : (c(r === 4, "only 2-byte and 4-byte strings are currently supported"), n = si, a = li, o = di), H(e, { name: t, fromWireType: (s) => {
      var l = (u(), h)[s >> 2], d = n(s + 4, l * r, !0);
      return J(s), d;
    }, toWireType: (s, l) => {
      typeof l != "string" && z(`Cannot pass non-string to C++ string type ${t}`);
      var d = o(l), m = Pe(4 + d + r);
      return (u(), h)[m >> 2] = d / r, a(l, m + 4, d + r), s !== null && s.push(J, m), m;
    }, readValueFromPointer: lr, destructorFunction(s) {
      J(s);
    } });
  }, ui = (e, r) => {
    r = B(r), H(e, { isVoid: !0, name: r, fromWireType: () => {
    }, toWireType: (t, n) => {
    } });
  }, fi = (e) => {
    _r(e, !ee, 1, !Ie, 65536, !1), k.threadInitTLS();
  }, ht = (e) => {
    if (e instanceof Rr || e == "unwind")
      return b(`handleException: unwinding: EXITSTATUS=${te}`), te;
    be(), e instanceof WebAssembly.RuntimeError && pr() <= 0 && w("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), Er(1, e);
  }, mi = () => {
    if (!Fe()) {
      b(`maybeExit: calling exit() implicitly after user callback completed: ${te}`);
      try {
        if (y) {
          Z() && hr(te);
          return;
        }
        rr(te);
      } catch (e) {
        ht(e);
      }
    }
  }, pt = (e) => {
    if (ue) {
      w("user callback triggered after runtime exited or application aborted.  Ignoring.");
      return;
    }
    try {
      e(), mi();
    } catch (r) {
      ht(r);
    }
  }, dr = (e) => {
    if (Atomics.waitAsync) {
      var r = Atomics.waitAsync((u(), I), e >> 2, e);
      c(r.async), r.value.then(Le);
      var t = e + 128;
      Atomics.store((u(), I), t >> 2, 1);
    }
  }, Le = () => pt(() => {
    var e = Z();
    e && (dr(e), zt());
  }), _i = (e, r) => {
    if (e == r)
      setTimeout(Le);
    else if (y)
      postMessage({ targetThread: e, cmd: "checkMailbox" });
    else {
      var t = k.pthreads[e];
      if (!t) {
        w(`Cannot send message to thread with ID ${e}, unknown thread ID!`);
        return;
      }
      t.postMessage({ cmd: "checkMailbox" });
    }
  }, Ue = [], vi = (e, r, t, n, a) => {
    n /= 2, Ue.length = n;
    for (var o = a >> 3, s = 0; s < n; s++)
      (u(), D)[o + 2 * s] ? Ue[s] = (u(), D)[o + 2 * s + 1] : Ue[s] = (u(), ke)[o + 2 * s + 1];
    var l = r ? fr[r] : Ki[e];
    c(!(e && r)), c(l.length == n, "Call args mismatch in _emscripten_receive_on_main_thread_js"), k.currentProxiedOperationCallerThread = t;
    var d = l(...Ue);
    return k.currentProxiedOperationCallerThread = 0, c(typeof d != "bigint"), d;
  }, hi = (e) => {
    b(`_emscripten_thread_cleanup: ${W(e)}`), y ? postMessage({ cmd: "cleanupThread", thread: e }) : Mr(e);
  }, pi = (e) => {
  }, gi = 9007199254740992, yi = -9007199254740992, cr = (e) => e < yi || e > gi ? NaN : Number(e);
  function gt(e, r, t, n, a, o, s) {
    if (y) return N(10, 0, 1, e, r, t, n, a, o, s);
    a = cr(a);
    try {
      c(!isNaN(a));
      var l = C.getStreamFromFD(n), d = i.mmap(l, e, a, r, t), m = d.ptr;
      return (u(), I)[o >> 2] = d.allocated, (u(), h)[s >> 2] = m, 0;
    } catch (v) {
      if (typeof i > "u" || v.name !== "ErrnoError") throw v;
      return -v.errno;
    }
  }
  function yt(e, r, t, n, a, o) {
    if (y) return N(11, 0, 1, e, r, t, n, a, o);
    o = cr(o);
    try {
      var s = C.getStreamFromFD(a);
      t & 2 && C.doMsync(e, s, r, n, o);
    } catch (l) {
      if (typeof i > "u" || l.name !== "ErrnoError") throw l;
      return -l.errno;
    }
  }
  var Ei = (e, r, t, n) => {
    var a = (/* @__PURE__ */ new Date()).getFullYear(), o = new Date(a, 0, 1), s = new Date(a, 6, 1), l = o.getTimezoneOffset(), d = s.getTimezoneOffset(), m = Math.max(l, d);
    (u(), h)[e >> 2] = m * 60, (u(), I)[r >> 2] = +(l != d);
    var v = (p) => {
      var T = p >= 0 ? "-" : "+", S = Math.abs(p), R = String(Math.floor(S / 60)).padStart(2, "0"), L = String(S % 60).padStart(2, "0");
      return `UTC${T}${R}${L}`;
    }, g = v(l), _ = v(d);
    c(g), c(_), c(ie(g) <= 16, `timezone name truncated to fit in TZNAME_MAX (${g})`), c(ie(_) <= 16, `timezone name truncated to fit in TZNAME_MAX (${_})`), d < l ? (V(g, t, 17), V(_, n, 17)) : (V(g, n, 17), V(_, t, 17));
  }, Et = () => performance.timeOrigin + performance.now(), wi = () => Date.now(), bi = (e) => e >= 0 && e <= 3;
  function Si(e, r, t) {
    if (!bi(e))
      return 28;
    var n;
    e === 0 ? n = wi() : n = Et();
    var a = Math.round(n * 1e3 * 1e3);
    return (u(), D)[t >> 3] = BigInt(a), 0;
  }
  var $e = [], ki = (e, r) => {
    c(Array.isArray($e)), c(r % 16 == 0), $e.length = 0;
    for (var t; t = (u(), $)[e++]; ) {
      var n = String.fromCharCode(t), a = ["d", "f", "i", "p"];
      a.push("j"), c(a.includes(n), `Invalid character ${t}("${n}") in readEmAsmArgs! Use only [${a}], and do not specify "v" for void return argument.`);
      var o = t != 105;
      o &= t != 112, r += o && r % 8 ? 4 : 0, $e.push(t == 112 ? (u(), h)[r >> 2] : t == 106 ? (u(), D)[r >> 3] : t == 105 ? (u(), I)[r >> 2] : (u(), ke)[r >> 3]), r += o ? 8 : 4;
    }
    return $e;
  }, Ti = (e, r, t) => {
    var n = ki(r, t);
    return c(fr.hasOwnProperty(e), `No EM_ASM constant found at address ${e}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), fr[e](...n);
  }, Fi = (e, r, t) => Ti(e, r, t), Ai = () => {
    ee || ne("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
  }, Pi = (e, r) => w(X(e, r)), wt = () => {
    K += 1, b(`runtimeKeepalivePush -> counter=${K}`);
  }, Ci = () => {
    throw wt(), "unwind";
  }, bt = () => 2147483648, Ii = () => bt(), Ri = () => navigator.hardwareConcurrency, ce = {}, St = (e) => {
    var r = ie(e) + 1, t = Pe(r);
    return t && V(e, t, r), t;
  }, Be = (e) => {
    var r;
    if (r = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(e))
      return +r[1];
    if (r = /\bwasm-function\[(\d+)\]:(\d+)/.exec(e))
      ne("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
    else if (r = /:(\d+):\d+(?:\)|$)/.exec(e))
      return 2147483648 | +r[1];
    return 0;
  }, kt = (e) => {
    for (var r of e) {
      var t = Be(r);
      t && (ce[t] = r);
    }
  }, Tt = () => new Error().stack.toString(), Di = () => {
    var e = Tt().split(`
`);
    return e[0] == "Error" && e.shift(), kt(e), ce.last_addr = Be(e[3]), ce.last_stack = e, ce.last_addr;
  }, ze = (e) => {
    var r = ce[e];
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
    return J(ze.ret ?? 0), ze.ret = St(t), ze.ret;
  }, Mi = (e) => {
    var r = j.buffer.byteLength, t = (e - r + 65535) / 65536 | 0;
    b(`growMemory: ${e} (+${e - r} bytes / ${t} pages)`);
    try {
      return j.grow(t), Ne(), 1;
    } catch (n) {
      w(`growMemory: Attempted to grow heap from ${r} bytes to ${e} bytes, but got error: ${n}`);
    }
  }, Ni = (e) => {
    var r = (u(), $).length;
    if (e >>>= 0, e <= r)
      return !1;
    var t = bt();
    if (e > t)
      return w(`Cannot enlarge memory, requested ${e} bytes, but the limit is ${t} bytes!`), !1;
    for (var n = 1; n <= 4; n *= 2) {
      var a = r * (1 + 0.2 / n);
      a = Math.min(a, e + 100663296);
      var o = Math.min(t, Yr(Math.max(e, a), 65536)), s = Mi(o);
      if (s)
        return !0;
    }
    return w(`Failed to grow the heap from ${r} bytes to ${o} bytes, not enough memory!`), !1;
  }, Oi = (e, r, t) => {
    var n;
    ce.last_addr == e ? n = ce.last_stack : (n = Tt().split(`
`), n[0] == "Error" && n.shift(), kt(n));
    for (var a = 3; n[a] && Be(n[a]) != e; )
      ++a;
    for (var o = 0; o < t && n[o + a]; ++o)
      (u(), I)[r + o * 4 >> 2] = Be(n[o + a]);
    return o;
  }, ur = {}, xi = () => yr || "./this.program", Ae = () => {
    if (!Ae.strings) {
      var e = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", r = { USER: "web_user", LOGNAME: "web_user", PATH: "/", PWD: "/", HOME: "/home/web_user", LANG: e, _: xi() };
      for (var t in ur)
        ur[t] === void 0 ? delete r[t] : r[t] = ur[t];
      var n = [];
      for (var t in r)
        n.push(`${t}=${r[t]}`);
      Ae.strings = n;
    }
    return Ae.strings;
  };
  function Ft(e, r) {
    if (y) return N(12, 0, 1, e, r);
    var t = 0, n = 0;
    for (var a of Ae()) {
      var o = r + t;
      (u(), h)[e + n >> 2] = o, t += V(a, o, 1 / 0) + 1, n += 4;
    }
    return 0;
  }
  function At(e, r) {
    if (y) return N(13, 0, 1, e, r);
    var t = Ae();
    (u(), h)[e >> 2] = t.length;
    var n = 0;
    for (var a of t)
      n += ie(a) + 1;
    return (u(), h)[r >> 2] = n, 0;
  }
  function Pt(e) {
    if (y) return N(14, 0, 1, e);
    try {
      var r = C.getStreamFromFD(e);
      return i.close(r), 0;
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var Wi = (e, r, t, n) => {
    for (var a = 0, o = 0; o < t; o++) {
      var s = (u(), h)[r >> 2], l = (u(), h)[r + 4 >> 2];
      r += 8;
      var d = i.read(e, (u(), O), s, l, n);
      if (d < 0) return -1;
      if (a += d, d < l) break;
    }
    return a;
  };
  function Ct(e, r, t, n) {
    if (y) return N(15, 0, 1, e, r, t, n);
    try {
      var a = C.getStreamFromFD(e), o = Wi(a, r, t);
      return (u(), h)[n >> 2] = o, 0;
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function It(e, r, t, n) {
    if (y) return N(16, 0, 1, e, r, t, n);
    r = cr(r);
    try {
      if (isNaN(r)) return 61;
      var a = C.getStreamFromFD(e);
      return i.llseek(a, r, t), (u(), D)[n >> 3] = BigInt(a.position), a.getdents && r === 0 && t === 0 && (a.getdents = null), 0;
    } catch (o) {
      if (typeof i > "u" || o.name !== "ErrnoError") throw o;
      return o.errno;
    }
  }
  var Li = (e, r, t, n) => {
    for (var a = 0, o = 0; o < t; o++) {
      var s = (u(), h)[r >> 2], l = (u(), h)[r + 4 >> 2];
      r += 8;
      var d = i.write(e, (u(), O), s, l, n);
      if (d < 0) return -1;
      if (a += d, d < l)
        break;
    }
    return a;
  };
  function Rt(e, r, t, n) {
    if (y) return N(17, 0, 1, e, r, t, n);
    try {
      var a = C.getStreamFromFD(e), o = Li(a, r, t);
      return (u(), h)[n >> 2] = o, 0;
    } catch (s) {
      if (typeof i > "u" || s.name !== "ErrnoError") throw s;
      return s.errno;
    }
  }
  function Ui(e, r) {
    try {
      return ir((u(), $).subarray(e, e + r)), 0;
    } catch (t) {
      if (typeof i > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  var $i = () => {
    c(K > 0), K -= 1, b(`runtimeKeepalivePop -> counter=${K}`);
  }, G = { instrumentWasmImports(e) {
    c("Suspending" in WebAssembly, "JSPI not supported by current environment. Perhaps it needs to be enabled via flags?");
    var r = /^(invoke_.*|__asyncjs__.*)$/;
    for (let [t, n] of Object.entries(e))
      typeof n == "function" && (n.isAsync || r.test(t)) && (e[t] = n = new WebAssembly.Suspending(n));
  }, instrumentFunction(e) {
    var r = (...t) => e(...t);
    return r = ft(`__asyncify_wrapper_${e.name}`, r), r;
  }, instrumentWasmExports(e) {
    var r = /^(solve_model|validate_model|main|__main_argc_argv)$/;
    G.asyncExports = /* @__PURE__ */ new Set();
    var t = {};
    for (let [a, o] of Object.entries(e))
      if (typeof o == "function") {
        r.test(a) && (G.asyncExports.add(o), o = G.makeAsyncFunction(o));
        var n = G.instrumentFunction(o);
        t[a] = n;
      } else
        t[a] = o;
    return t;
  }, asyncExports: null, isAsyncExport(e) {
    return G.asyncExports?.has(e);
  }, handleAsync: async (e) => {
    wt();
    try {
      return await e();
    } finally {
      $i();
    }
  }, handleSleep: (e) => G.handleAsync(() => new Promise(e)), makeAsyncFunction(e) {
    return WebAssembly.promising(e);
  } }, Bi = (e) => {
    var r = f["_" + e];
    return c(r, "Cannot call unknown function " + e + ", make sure it is exported"), r;
  }, zi = (e, r) => {
    c(e.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (u(), O).set(e, r);
  }, ji = (e) => {
    var r = ie(e) + 1, t = Qe(r);
    return V(e, t, r), t;
  }, Dt = (e, r, t, n, a) => {
    var o = { string: (S) => {
      var R = 0;
      return S != null && S !== 0 && (R = ji(S)), R;
    }, array: (S) => {
      var R = Qe(S.length);
      return zi(S, R), R;
    } };
    function s(S) {
      return r === "string" ? X(S) : r === "boolean" ? !!S : S;
    }
    var l = Bi(e), d = [], m = 0;
    if (c(r !== "array", 'Return type should not be "array".'), n)
      for (var v = 0; v < n.length; v++) {
        var g = o[t[v]];
        g ? (m === 0 && (m = $r()), d[v] = g(n[v])) : d[v] = n[v];
      }
    var _ = l(...d);
    function p(S) {
      return m !== 0 && Ze(m), s(S);
    }
    var T = a?.async;
    return T ? _.then(p) : (_ = p(_), _);
  }, Hi = (e, r, t, n) => (...a) => Dt(e, r, t, a, n), Vi = (...e) => St(...e);
  k.init(), i.createPreloadedFile = An, i.preloadFile = et, i.staticInit(), c(Y.length === 10);
  {
    if (rn(), f.noExitRuntime && (nr = f.noExitRuntime), f.preloadPlugins && (Qr = f.preloadPlugins), f.print && (re = f.print), f.printErr && (w = f.printErr), f.wasmBinary && (we = f.wasmBinary), Xi(), f.arguments && f.arguments, f.thisProgram && (yr = f.thisProgram), c(typeof f.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), c(typeof f.read > "u", "Module.read option was removed"), c(typeof f.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), c(typeof f.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), c(typeof f.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), c(typeof f.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), c(typeof f.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), c(typeof f.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), f.preInit)
      for (typeof f.preInit == "function" && (f.preInit = [f.preInit]); f.preInit.length > 0; )
        f.preInit.shift()();
    Me("preInit");
  }
  f.ccall = Dt, f.cwrap = Hi, f.UTF8ToString = X, f.stringToUTF8 = V, f.allocateUTF8 = Vi;
  var Gi = ["writeI53ToI64", "writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "convertU32PairToI53", "getTempRet0", "setTempRet0", "withStackSave", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "runMainThreadEmAsm", "jstoi_q", "autoResumeAudioContext", "getDynCaller", "dynCall", "asmjsMangle", "HandleAllocator", "addOnInit", "addOnPostCtor", "addOnPreMain", "addOnExit", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "intArrayToString", "stringToAscii", "registerKeyEventCallback", "maybeCStringToJsString", "findEventTarget", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "registerBatteryEventCallback", "setCanvasElementSizeCallingThread", "setCanvasElementSizeMainThread", "setCanvasElementSize", "getCanvasSizeCallingThread", "getCanvasSizeMainThread", "getCanvasElementSize", "getCallstack", "convertPCtoSourceLocation", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "safeSetTimeout", "setImmediateWrapped", "safeRequestAnimationFrame", "clearImmediateWrapped", "registerPostMainLoop", "registerPreMainLoop", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "isLeapYear", "ydayFromDate", "arraySum", "addDays", "getSocketFromFD", "getSocketAddress", "FS_mkdirTree", "_setNetworkCallback", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "webgl_enable_EXT_polygon_offset_clamp", "webgl_enable_EXT_clip_control", "webgl_enable_WEBGL_polygon_mode", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "emscriptenWebGLGetUniform", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "emscripten_webgl_destroy_context_before_on_calling_thread", "registerWebGlEventCallback", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "allocateUTF8OnStack", "demangle", "stackTrace", "getNativeTypeSize", "getFunctionArgsName", "requireRegisteredType", "createJsInvokerSignature", "PureVirtualError", "getBasestPointer", "registerInheritedInstance", "unregisterInheritedInstance", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "enumReadValueFromPointer", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "flushPendingDeletes", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "count_emval_handles", "getStringOrSymbol", "emval_returnValue", "emval_lookupTypes", "emval_addMethodCaller"];
  Gi.forEach(Qt);
  var qi = ["run", "out", "err", "callMain", "abort", "wasmExports", "HEAPF32", "HEAPF64", "HEAP8", "HEAP16", "HEAPU16", "HEAP32", "HEAPU32", "HEAP64", "HEAPU64", "writeStackCookie", "checkStackCookie", "prettyPrint", "INT53_MAX", "INT53_MIN", "bigintToI53Checked", "stackSave", "stackRestore", "stackAlloc", "createNamedFunction", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "ERRNO_CODES", "strError", "DNS", "Protocols", "Sockets", "timers", "warnOnce", "readEmAsmArgsArray", "readEmAsmArgs", "runEmAsmFunction", "getExecutableName", "handleException", "keepRuntimeAlive", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "wasmMemory", "getUniqueRunDependency", "noExitRuntime", "addRunDependency", "removeRunDependency", "addOnPreRun", "addOnPostRun", "freeTableIndexes", "functionsInTableMap", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "stringToUTF8Array", "lengthBytesUTF8", "intArrayFromString", "AsciiToString", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "stringToUTF8OnStack", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "jsStackTrace", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "checkWasiClock", "doReadv", "doWritev", "initRandomFill", "randomFill", "emSetImmediate", "emClearImmediate_deps", "emClearImmediate", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "requestFullscreen", "requestFullScreen", "setCanvasSize", "getUserMedia", "createContext", "getPreloadedImageData__data", "wget", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_preloadFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS_unlink", "FS_createPath", "FS_createDevice", "FS_readFile", "FS", "FS_root", "FS_mounts", "FS_devices", "FS_streams", "FS_nextInode", "FS_nameTable", "FS_currentPath", "FS_initialized", "FS_ignorePermissions", "FS_filesystems", "FS_syncFSRequests", "FS_readFiles", "FS_lookupPath", "FS_getPath", "FS_hashName", "FS_hashAddNode", "FS_hashRemoveNode", "FS_lookupNode", "FS_createNode", "FS_destroyNode", "FS_isRoot", "FS_isMountpoint", "FS_isFile", "FS_isDir", "FS_isLink", "FS_isChrdev", "FS_isBlkdev", "FS_isFIFO", "FS_isSocket", "FS_flagsToPermissionString", "FS_nodePermissions", "FS_mayLookup", "FS_mayCreate", "FS_mayDelete", "FS_mayOpen", "FS_checkOpExists", "FS_nextfd", "FS_getStreamChecked", "FS_getStream", "FS_createStream", "FS_closeStream", "FS_dupStream", "FS_doSetAttr", "FS_chrdev_stream_ops", "FS_major", "FS_minor", "FS_makedev", "FS_registerDevice", "FS_getDevice", "FS_getMounts", "FS_syncfs", "FS_mount", "FS_unmount", "FS_lookup", "FS_mknod", "FS_statfs", "FS_statfsStream", "FS_statfsNode", "FS_create", "FS_mkdir", "FS_mkdev", "FS_symlink", "FS_rename", "FS_rmdir", "FS_readdir", "FS_readlink", "FS_stat", "FS_fstat", "FS_lstat", "FS_doChmod", "FS_chmod", "FS_lchmod", "FS_fchmod", "FS_doChown", "FS_chown", "FS_lchown", "FS_fchown", "FS_doTruncate", "FS_truncate", "FS_ftruncate", "FS_utime", "FS_open", "FS_close", "FS_isClosed", "FS_llseek", "FS_read", "FS_write", "FS_mmap", "FS_msync", "FS_ioctl", "FS_writeFile", "FS_cwd", "FS_chdir", "FS_createDefaultDirectories", "FS_createDefaultDevices", "FS_createSpecialDirectories", "FS_createStandardStreams", "FS_staticInit", "FS_init", "FS_quit", "FS_findObject", "FS_analyzePath", "FS_createFile", "FS_createDataFile", "FS_forceLoadFile", "FS_createLazyFile", "FS_absolutePath", "FS_createFolder", "FS_createLink", "FS_joinPath", "FS_mmapAlloc", "FS_standardizePath", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "GL", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "runAndAbortIfError", "Asyncify", "Fibers", "SDL", "SDL_gfx", "print", "printErr", "jstoi_s", "PThread", "terminateWorker", "cleanupThread", "registerTLSInit", "spawnThread", "exitOnMainThread", "proxyToMainThread", "proxiedJSCallArgs", "invokeEntryPoint", "checkMailbox", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "getTypeName", "getFunctionName", "heap32VectorToArray", "usesDestructorStack", "checkArgCount", "getRequiredArgCount", "createJsInvoker", "UnboundTypeError", "EmValType", "EmValOptionalType", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "embindRepr", "registeredInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "floatReadValueFromPointer", "assertIntegerRange", "readPointer", "runDestructors", "craftInvokerFunction", "embind__requireFunction", "finalizationRegistry", "detachFinalizer_deps", "deletionQueue", "delayFunction", "emval_freelist", "emval_handles", "emval_symbols", "Emval", "emval_methodCallers"];
  qi.forEach(Sr);
  var Ki = [er, Br, qr, rt, tt, nt, it, at, ot, st, gt, yt, Ft, At, Pt, Ct, It, Rt];
  function Xi() {
    Jt("fetchSettings");
  }
  var fr = { 541060: () => typeof wasmOffsetConverter < "u" };
  function Mt() {
    var e = "";
    typeof location < "u" && location.hostname && (e = location.hostname);
    var r = ie(e) + 1, t = Pe(r);
    return V(e, t, r), t;
  }
  Mt.sig = "i";
  function Nt() {
    return typeof wasmOffsetConverter < "u";
  }
  Nt.sig = "i";
  var Ot = A("___getTypeName"), xt = A("__embind_initialize_bindings");
  f._get_cp_model_schema = A("_get_cp_model_schema"), f._get_sat_parameters_schema = A("_get_sat_parameters_schema"), f._solve_model = A("_solve_model");
  var Pe = f._malloc = A("_malloc");
  f._free_buffer = A("_free_buffer");
  var J = f._free = A("_free");
  f._interrupt_solve = A("_interrupt_solve"), f._validate_model = A("_validate_model");
  var Z = A("_pthread_self"), mr = A("_fflush"), Wt = A("_strerror"), Lt = A("_emscripten_builtin_memalign"), _r = A("__emscripten_thread_init"), Ut = A("__emscripten_thread_crashed"), vr = A("_emscripten_stack_get_end"), $t = A("__emscripten_run_js_on_main_thread"), Bt = A("__emscripten_thread_free_data"), hr = A("__emscripten_thread_exit"), zt = A("__emscripten_check_mailbox"), jt = A("_emscripten_stack_init"), Ht = A("_emscripten_stack_set_limits"), Vt = A("__emscripten_stack_restore"), Gt = A("__emscripten_stack_alloc"), pr = A("_emscripten_stack_get_current"), qt = A("wasmTable");
  function Yi(e) {
    c(typeof e.__getTypeName < "u", "missing Wasm export: __getTypeName"), c(typeof e._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), c(typeof e.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), c(typeof e.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), c(typeof e.solve_model < "u", "missing Wasm export: solve_model"), c(typeof e.malloc < "u", "missing Wasm export: malloc"), c(typeof e.free_buffer < "u", "missing Wasm export: free_buffer"), c(typeof e.free < "u", "missing Wasm export: free"), c(typeof e.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), c(typeof e.validate_model < "u", "missing Wasm export: validate_model"), c(typeof e.pthread_self < "u", "missing Wasm export: pthread_self"), c(typeof e.fflush < "u", "missing Wasm export: fflush"), c(typeof e.strerror < "u", "missing Wasm export: strerror"), c(typeof e._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), c(typeof e.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), c(typeof e._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), c(typeof e._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), c(typeof e.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), c(typeof e.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), c(typeof e._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), c(typeof e._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), c(typeof e._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), c(typeof e._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), c(typeof e.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), c(typeof e.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), c(typeof e.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), c(typeof e._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), c(typeof e._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), c(typeof e.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), c(typeof e.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), Ot = M("__getTypeName", 1), xt = M("_embind_initialize_bindings", 0), f._get_cp_model_schema = M("get_cp_model_schema", 0), f._get_sat_parameters_schema = M("get_sat_parameters_schema", 0), f._solve_model = M("solve_model", 5), Pe = f._malloc = M("malloc", 1), f._free_buffer = M("free_buffer", 1), J = f._free = M("free", 1), f._interrupt_solve = M("interrupt_solve", 0), f._validate_model = M("validate_model", 3), Z = M("pthread_self", 0), mr = M("fflush", 1), Wt = M("strerror", 1), Lt = M("emscripten_builtin_memalign", 2), _r = M("_emscripten_thread_init", 6), Ut = M("_emscripten_thread_crashed", 0), vr = e.emscripten_stack_get_end, e.emscripten_stack_get_base, $t = M("_emscripten_run_js_on_main_thread", 5), Bt = M("_emscripten_thread_free_data", 1), hr = M("_emscripten_thread_exit", 1), zt = M("_emscripten_check_mailbox", 0), jt = e.emscripten_stack_init, Ht = e.emscripten_stack_set_limits, e.emscripten_stack_get_free, Vt = e._emscripten_stack_restore, Gt = e._emscripten_stack_alloc, pr = e.emscripten_stack_get_current, qt = e.__indirect_function_table;
  }
  var ye;
  function Ji() {
    ye = { GetHostCString: Mt, HaveOffsetConverter: Nt, __assert_fail: _n, __cxa_throw: hn, __pthread_create_js: Kr, __syscall_fcntl64: rt, __syscall_fstat64: tt, __syscall_ioctl: nt, __syscall_lstat64: it, __syscall_newfstatat: at, __syscall_openat: ot, __syscall_stat64: st, _abort_js: Pn, _embind_register_bigint: Rn, _embind_register_bool: Dn, _embind_register_emval: On, _embind_register_float: Wn, _embind_register_function: ei, _embind_register_integer: ri, _embind_register_memory_view: ti, _embind_register_std_string: ni, _embind_register_std_wstring: ci, _embind_register_void: ui, _emscripten_init_main_thread_js: fi, _emscripten_notify_mailbox_postmessage: _i, _emscripten_receive_on_main_thread_js: vi, _emscripten_thread_cleanup: hi, _emscripten_thread_mailbox_await: dr, _emscripten_thread_set_strongref: pi, _mmap_js: gt, _munmap_js: yt, _tzset_js: Ei, clock_time_get: Si, emscripten_asm_const_int: Fi, emscripten_check_blocking_allowed: Ai, emscripten_errn: Pi, emscripten_exit_with_live_runtime: Ci, emscripten_get_heap_max: Ii, emscripten_get_now: Et, emscripten_num_logical_cores: Ri, emscripten_pc_get_function: ze, emscripten_resize_heap: Ni, emscripten_stack_snapshot: Di, emscripten_stack_unwind_buffer: Oi, environ_get: Ft, environ_sizes_get: At, exit: rr, fd_close: Pt, fd_read: Ct, fd_seek: It, fd_write: Rt, memory: j, proc_exit: er, random_get: Ui };
  }
  var Kt;
  function Zi() {
    c(!y), jt(), br();
  }
  function je() {
    if (le > 0) {
      b("run() called, but dependencies remain, so not running"), Te = je;
      return;
    }
    if (y) {
      Ke?.(f), Pr();
      return;
    }
    if (Zi(), tn(), le > 0) {
      b("run() called, but dependencies remain, so not running"), Te = je;
      return;
    }
    async function e() {
      c(!Kt), Kt = !0, f.calledRun = !0, !ue && (Pr(), Ke?.(f), f.onRuntimeInitialized?.(), Me("onRuntimeInitialized"), c(!f._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), nn());
    }
    f.setStatus ? (f.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => f.setStatus(""), 1), e();
    }, 1)) : e(), be();
  }
  function Qi() {
    var e = re, r = w, t = !1;
    re = w = (d) => {
      t = !0;
    };
    try {
      mr(0);
      for (var n of ["stdout", "stderr"]) {
        var a = i.analyzePath("/dev/" + n);
        if (!a) return;
        var o = a.object, s = o.rdev, l = ae.ttys[s];
        l?.output?.length && (t = !0);
      }
    } catch {
    }
    re = e, w = r, t && ne("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
  }
  var Q;
  y || (Q = await Ir(), je()), fe ? Ve = f : Ve = new Promise((e, r) => {
    Ke = e, Xe = r;
  });
  for (const e of Object.keys(f))
    e in He || Object.defineProperty(He, e, { configurable: !0, get() {
      P(`Access to module property ('${e}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
    } });
  return Ve;
}
var ra = globalThis.self?.name?.startsWith("em-pthread");
ra && ea();
//# sourceMappingURL=cp_sat_runtime-CFluDBLY.js.map
