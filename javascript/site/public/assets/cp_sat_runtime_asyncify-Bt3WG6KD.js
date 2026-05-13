async function bb(El = {}) {
  var Ml;
  (function() {
    function l(W) {
      W = W.split("-")[0];
      for (var X = W.split(".").slice(0, 3); X.length < 3; ) X.push("00");
      return X = X.map((h, s, G) => h.padStart(2, "0")), X.join("");
    }
    var Z = (W) => [W / 1e4 | 0, (W / 100 | 0) % 100, W % 100].join("."), c = 2147483647, d = typeof process < "u" && process.versions?.node ? l(process.versions.node) : c;
    if (d < c)
      throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    if (d < 2147483647)
      throw new Error(`This emscripten-generated code requires node v${Z(2147483647)} (detected v${Z(d)})`);
    var b = typeof navigator < "u" && navigator.userAgent;
    if (b) {
      var m = b.includes("Safari/") && b.match(/Version\/(\d+\.?\d*\.?\d*)/) ? l(b.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : c;
      if (m < 15e4)
        throw new Error(`This emscripten-generated code requires Safari v${Z(15e4)} (detected v${m})`);
      var t = b.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(b.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : c;
      if (t < 114)
        throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${t})`);
      var i = b.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(b.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : c;
      if (i < 85)
        throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${i})`);
    }
  })();
  var V = El, vl = !!globalThis.window, $ = !!globalThis.WorkerGlobalScope, jl = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", uZ = !vl && !jl && !$, p = $ && self.name?.startsWith("em-pthread");
  p && (n(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0);
  var pZ = "./this.program", FZ = (l, Z) => {
    throw Z;
  }, $c = import.meta.url, Pl = "";
  function qc(l) {
    return V.locateFile ? V.locateFile(l, Pl) : Pl + l;
  }
  var _l, Ul;
  if (!uZ) if (vl || $) {
    try {
      Pl = new URL(".", $c).href;
    } catch {
    }
    if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    $ && (Ul = (l) => {
      var Z = new XMLHttpRequest();
      return Z.open("GET", l, !1), Z.responseType = "arraybuffer", Z.send(null), new Uint8Array(Z.response);
    }), _l = async (l) => {
      n(!YZ(l), "readAsync does not work with file:// URLs");
      var Z = await fetch(l, { credentials: "same-origin" });
      if (Z.ok)
        return Z.arrayBuffer();
      throw new Error(Z.status + " : " + Z.url);
    };
  } else
    throw new Error("environment detection error");
  var q = console.log.bind(console), Y = console.error.bind(console);
  n(vl || $ || jl, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), n(!jl, "node environment detected but not enabled at build time.  Add `node` to `-sENVIRONMENT` to enable."), n(!uZ, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
  var ul;
  globalThis.WebAssembly || Y("no native wasm support detected");
  var cl, P = !1, nl;
  function n(l, Z) {
    l || v("Assertion failed" + (Z ? ": " + Z : ""));
  }
  var YZ = (l) => l.startsWith("file://");
  function IZ() {
    var l = RZ();
    n((l & 3) == 0), l == 0 && (l += 4), (a(), R)[l >> 2] = 34821223, (a(), R)[l + 4 >> 2] = 2310721022, (a(), R)[0] = 1668509029;
  }
  function pl() {
    if (!P) {
      var l = RZ();
      l == 0 && (l += 4);
      var Z = (a(), R)[l >> 2], c = (a(), R)[l + 4 >> 2];
      (Z != 34821223 || c != 2310721022) && v(`Stack overflow! Stack cookie has been overwritten at ${ml(l)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${ml(c)} ${ml(Z)}`), (a(), R)[0] != 1668509029 && v("Runtime error: The application has corrupted its heap memory area (address zero)!");
    }
  }
  function Ol(...l) {
    console.warn(...l);
  }
  (() => {
    var l = new Int16Array(1), Z = new Int8Array(l.buffer);
    l[0] = 25459, (Z[0] !== 115 || Z[1] !== 99) && v("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
  })();
  function Hl(l) {
    Object.getOwnPropertyDescriptor(V, l) || Object.defineProperty(V, l, { configurable: !0, set() {
      v(`Attempt to set \`Module.${l}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
    } });
  }
  function N(l) {
    return () => n(!1, `call to '${l}' via reference taken before Wasm module initialization`);
  }
  function ld(l) {
    Object.getOwnPropertyDescriptor(V, l) && v(`\`Module.${l}\` was supplied but \`${l}\` not included in INCOMING_MODULE_JS_API`);
  }
  function Zd(l) {
    return l === "FS_createPath" || l === "FS_createDataFile" || l === "FS_createPreloadedFile" || l === "FS_preloadFile" || l === "FS_unlink" || l === "addRunDependency" || l === "FS_createLazyFile" || l === "FS_createDevice" || l === "removeRunDependency";
  }
  function cd(l) {
    JZ(l);
  }
  function JZ(l) {
    p || Object.getOwnPropertyDescriptor(V, l) || Object.defineProperty(V, l, { configurable: !0, get() {
      var Z = `'${l}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
      Zd(l) && (Z += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), v(Z);
    } });
  }
  function dd() {
    function l() {
      var c = 0;
      return Wl && typeof Rl < "u" && (c = Rl()), `w:${NZ},t:${ml(c)}:`;
    }
    var Z = Ol;
    Ol = (...c) => Z(l(), ...c);
  }
  dd();
  function a() {
    L.buffer != S.buffer && Tl();
  }
  var Dl, Al, NZ = 0, kZ;
  if (p) {
    let l = function(Z) {
      try {
        var c = Z.data, d = c.cmd;
        if (d === "load") {
          NZ = c.workerID;
          let b = [];
          self.onmessage = (m) => b.push(m), kZ = () => {
            postMessage({ cmd: "loaded" });
            for (let m of b)
              l(m);
            self.onmessage = l;
          };
          for (const m of c.handlers)
            (!V[m] || V[m].proxy) && (V[m] = (...t) => {
              postMessage({ cmd: "callHandler", handler: m, args: t });
            }, m == "print" && (q = V[m]), m == "printErr" && (Y = V[m]));
          L = c.wasmMemory, Tl(), cl = c.wasmModule, fZ(), Ll();
        } else if (d === "run") {
          n(c.pthread_ptr), Gd(c.pthread_ptr), hZ(c.pthread_ptr, 0, 0, 1, 0, 0), I.threadInitTLS(), VZ(c.pthread_ptr), $l || (Bc(), $l = !0);
          try {
            jZ(c.start_routine, c.arg);
          } catch (b) {
            if (b != "unwind")
              throw b;
          }
        } else c.target === "setimmediate" || (d === "checkMailbox" ? $l && xl() : d && (Y(`worker: received unknown command ${d}`), Y(c)));
      } catch (b) {
        throw Y(`worker: onmessage() captured an uncaught exception: ${b}`), b?.stack && Y(b.stack), xc(), b;
      }
    };
    var $l = !1;
    self.onunhandledrejection = (Z) => {
      throw Z.reason || Z;
    }, self.onmessage = l;
  }
  var S, w, dl, Fl, f, R, vZ, Yl, B, UZ, Wl = !1;
  function Tl() {
    var l = L.buffer;
    S = new Int8Array(l), dl = new Int16Array(l), V.HEAPU8 = w = new Uint8Array(l), Fl = new Uint16Array(l), f = new Int32Array(l), R = new Uint32Array(l), vZ = new Float32Array(l), Yl = new Float64Array(l), B = new BigInt64Array(l), UZ = new BigUint64Array(l);
  }
  function ed() {
    if (!p) {
      if (V.wasmMemory)
        L = V.wasmMemory;
      else {
        var l = V.INITIAL_MEMORY || 16777216;
        n(l >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + l + "! (STACK_SIZE=65536)"), L = new WebAssembly.Memory({ initial: l / 65536, maximum: 32768, shared: !0 });
      }
      Tl();
    }
  }
  n(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
  function bd() {
    if (n(!p), V.preRun)
      for (typeof V.preRun == "function" && (V.preRun = [V.preRun]); V.preRun.length; )
        KZ(V.preRun.shift());
    Hl("preRun"), SZ(xZ);
  }
  function HZ() {
    if (n(!Wl), Wl = !0, p) return kZ();
    pl(), !V.noFSInit && !e.initialized && e.init(), A.__wasm_call_ctors(), e.ignorePermissions = !1;
  }
  function md() {
    if (pl(), !p) {
      if (V.postRun)
        for (typeof V.postRun == "function" && (V.postRun = [V.postRun]); V.postRun.length; )
          Xd(V.postRun.shift());
      Hl("postRun"), SZ(MZ);
    }
  }
  function v(l) {
    V.onAbort?.(l), l = "Aborted(" + l + ")", Y(l), P = !0, l.indexOf("RuntimeError: unreachable") >= 0 && (l += '. "unreachable" may be due to ASYNCIFY_STACK_SIZE not being large enough (try increasing it)');
    var Z = new WebAssembly.RuntimeError(l);
    throw Al?.(Z), Z;
  }
  function y(l, Z) {
    return (...c) => {
      n(Wl, `native function \`${l}\` called before runtime initialization`);
      var d = A[l];
      return n(d, `exported native function \`${l}\` not found`), n(c.length <= Z, `native function \`${l}\` called with ${c.length} args but expects ${Z}`), d(...c);
    };
  }
  var ql;
  function td() {
    return V.locateFile ? qc("cp_sat_runtime_asyncify.wasm") : new URL("/assets/cp_sat_runtime_asyncify-DMLrX9Z5.wasm", import.meta.url).href;
  }
  function id(l) {
    if (l == ql && ul)
      return new Uint8Array(ul);
    if (Ul)
      return Ul(l);
    throw "both async and sync fetching of the wasm failed";
  }
  async function nd(l) {
    if (!ul)
      try {
        var Z = await _l(l);
        return new Uint8Array(Z);
      } catch {
      }
    return id(l);
  }
  async function Wd(l, Z) {
    try {
      var c = await nd(l), d = await WebAssembly.instantiate(c, Z);
      return d;
    } catch (b) {
      Y(`failed to asynchronously prepare wasm: ${b}`), YZ(l) && Y(`warning: Loading from a file URI (${l}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), v(b);
    }
  }
  async function ad(l, Z, c) {
    if (!l)
      try {
        var d = fetch(Z, { credentials: "same-origin" }), b = await WebAssembly.instantiateStreaming(d, c);
        return b;
      } catch (m) {
        Y(`wasm streaming compile failed: ${m}`), Y("falling back to ArrayBuffer instantiation");
      }
    return Wd(Z, c);
  }
  function TZ() {
    cb(), ol.__instrumented || (ol.__instrumented = !0, r.instrumentWasmImports(ol));
    var l = { env: ol, wasi_snapshot_preview1: ol };
    return l;
  }
  async function fZ() {
    function l(i, W) {
      return A = i.exports, A = r.instrumentWasmExports(A), hd(A._emscripten_tls_init), Zb(A), cl = W, A;
    }
    var Z = V;
    function c(i) {
      return n(V === Z, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), Z = null, l(i.instance, i.module);
    }
    var d = TZ();
    if (V.instantiateWasm)
      return new Promise((i, W) => {
        try {
          V.instantiateWasm(d, (X, h) => {
            i(l(X, h));
          });
        } catch (X) {
          Y(`Module.instantiateWasm callback failed with error: ${X}`), W(X);
        }
      });
    if (p) {
      n(cl, "wasmModule should have been received via postMessage");
      var b = new WebAssembly.Instance(cl, TZ());
      return l(b, cl);
    }
    ql ??= td();
    var m = await ad(ul, ql, d), t = c(m);
    return t;
  }
  class gZ {
    name = "ExitStatus";
    constructor(Z) {
      this.message = `Program terminated with exit(${Z})`, this.status = Z;
    }
  }
  var BZ = (l) => {
    l.terminate(), l.onmessage = (Z) => {
      var c = Z.data.cmd;
      Y(`received "${c}" command from terminated worker: ${l.workerID}`);
    };
  }, zZ = (l) => {
    n(!p, "Internal Error! cleanupThread() can only ever be called from main application thread!"), n(l, "Internal Error! Null pthread_ptr in cleanupThread!");
    var Z = I.pthreads[l];
    n(Z), I.returnWorkerToPool(Z);
  }, SZ = (l) => {
    for (; l.length > 0; )
      l.shift()(V);
  }, xZ = [], KZ = (l) => xZ.push(l), el = 0, Il = null, al = {}, bl = null, wZ = (l) => {
    if (el--, V.monitorRunDependencies?.(el), n(l, "removeRunDependency requires an ID"), n(al[l]), delete al[l], el == 0 && (bl !== null && (clearInterval(bl), bl = null), Il)) {
      var Z = Il;
      Il = null, Z();
    }
  }, CZ = (l) => {
    el++, V.monitorRunDependencies?.(el), n(l, "addRunDependency requires an ID"), n(!al[l]), al[l] = 1, bl === null && globalThis.setInterval && (bl = setInterval(() => {
      if (P) {
        clearInterval(bl), bl = null;
        return;
      }
      var Z = !1;
      for (var c in al)
        Z || (Z = !0, Y("still waiting on run dependencies:")), Y(`dependency: ${c}`);
      Z && Y("(end of list)");
    }, 1e4));
  }, QZ = (l) => {
    n(!p, "Internal Error! spawnThread() can only ever be called from main application thread!"), n(l.pthread_ptr, "Internal error, no pthread ptr!");
    var Z = I.getNewWorker();
    if (!Z)
      return 6;
    n(!Z.pthread_ptr, "Internal error!"), I.runningWorkers.push(Z), I.pthreads[l.pthread_ptr] = Z, Z.pthread_ptr = l.pthread_ptr;
    var c = { cmd: "run", start_routine: l.startRoutine, arg: l.arg, pthread_ptr: l.pthread_ptr };
    return Z.postMessage(c, l.transferList), 0;
  }, Vl = 0, fl = () => eZ || Vl > 0, LZ = () => sZ(), lZ = (l) => Ec(l), ZZ = (l) => Mc(l), z = (l, Z, c, ...d) => {
    for (var b = d.length * 2, m = LZ(), t = ZZ(b * 8), i = t >> 3, W = 0; W < d.length; W++) {
      var X = d[W];
      typeof X == "bigint" ? ((a(), B)[i + 2 * W] = 1n, (a(), B)[i + 2 * W + 1] = X) : ((a(), B)[i + 2 * W] = 0n, (a(), Yl)[i + 2 * W + 1] = X);
    }
    var h = Kc(l, Z, b, t, c);
    return lZ(m), h;
  };
  function cZ(l) {
    if (p) return z(0, 0, 1, l);
    nl = l, fl() || (I.terminateAllThreads(), V.onExit?.(l), P = !0), FZ(l, new gZ(l));
  }
  function EZ(l) {
    if (p) return z(1, 0, 0, l);
    dZ(l);
  }
  var Vd = (l, Z) => {
    if (nl = l, eb(), p)
      throw n(!Z), EZ(l), "unwind";
    if (fl() && !Z) {
      var c = `program exited (with status: ${l}), but keepRuntimeAlive() is set (counter=${Vl}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
      Al?.(c), Y(c);
    }
    cZ(l);
  }, dZ = Vd, ml = (l) => (n(typeof l == "number", `ptrToString expects a number, got ${typeof l}`), l >>>= 0, "0x" + l.toString(16).padStart(8, "0")), I = { unusedWorkers: [], runningWorkers: [], tlsInitFunctions: [], pthreads: {}, nextWorkerID: 1, init() {
    p || I.initMainThread();
  }, initMainThread() {
    for (var l = navigator.hardwareConcurrency; l--; )
      I.allocateUnusedWorker();
    KZ(async () => {
      var Z = I.loadWasmModuleToAllWorkers();
      CZ("loading-workers"), await Z, wZ("loading-workers");
    });
  }, terminateAllThreads: () => {
    n(!p, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
    for (var l of I.runningWorkers)
      BZ(l);
    for (var l of I.unusedWorkers)
      BZ(l);
    I.unusedWorkers = [], I.runningWorkers = [], I.pthreads = {};
  }, returnWorkerToPool: (l) => {
    var Z = l.pthread_ptr;
    delete I.pthreads[Z], I.unusedWorkers.push(l), I.runningWorkers.splice(I.runningWorkers.indexOf(l), 1), l.pthread_ptr = 0, wc(Z);
  }, threadInitTLS() {
    I.tlsInitFunctions.forEach((l) => l());
  }, loadWasmModuleToWorker: (l) => new Promise((Z) => {
    l.onmessage = (m) => {
      var t = m.data, i = t.cmd;
      if (t.targetThread && t.targetThread != Rl()) {
        var W = I.pthreads[t.targetThread];
        W ? W.postMessage(t, t.transferList) : Y(`Internal error! Worker sent a message "${i}" to target pthread ${t.targetThread}, but that thread no longer exists!`);
        return;
      }
      i === "checkMailbox" ? xl() : i === "spawnThread" ? QZ(t) : i === "cleanupThread" ? WZ(() => zZ(t.thread)) : i === "loaded" ? (l.loaded = !0, Z(l)) : t.target === "setimmediate" ? l.postMessage(t) : i === "callHandler" ? V[t.handler](...t.args) : i && Y(`worker sent an unknown command ${i}`);
    }, l.onerror = (m) => {
      var t = "worker sent an error!";
      throw l.pthread_ptr && (t = `Pthread ${ml(l.pthread_ptr)} sent an error!`), Y(`${t} ${m.filename}:${m.lineno}: ${m.message}`), m;
    }, n(L instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), n(cl instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
    var c = [], d = ["onExit", "onAbort", "print", "printErr"];
    for (var b of d)
      V.propertyIsEnumerable(b) && c.push(b);
    l.postMessage({ cmd: "load", handlers: c, wasmMemory: L, wasmModule: cl, workerID: l.workerID });
  }), async loadWasmModuleToAllWorkers() {
    return p ? void 0 : Promise.all(I.unusedWorkers.map(I.loadWasmModuleToWorker));
  }, allocateUnusedWorker() {
    var l;
    if (V.mainScriptUrlOrBlob) {
      var Z = V.mainScriptUrlOrBlob;
      typeof Z != "string" && (Z = URL.createObjectURL(Z)), l = new Worker(
        Z,
        /* @vite-ignore */
        { type: "module", name: "em-pthread-" + I.nextWorkerID }
      );
    } else l = new Worker(
      /* @vite-ignore */
      new URL("data:text/javascript;base64,YXN5bmMgZnVuY3Rpb24gY3BTYXRNb2R1bGUobW9kdWxlQXJnPXt9KXt2YXIgbW9kdWxlUnRuOyhmdW5jdGlvbigpe2Z1bmN0aW9uIGh1bWFuUmVhZGFibGVWZXJzaW9uVG9QYWNrZWQoc3RyKXtzdHI9c3RyLnNwbGl0KCItIilbMF07dmFyIHZlcnM9c3RyLnNwbGl0KCIuIikuc2xpY2UoMCwzKTt3aGlsZSh2ZXJzLmxlbmd0aDwzKXZlcnMucHVzaCgiMDAiKTt2ZXJzPXZlcnMubWFwKChuLGksYXJyKT0+bi5wYWRTdGFydCgyLCIwIikpO3JldHVybiB2ZXJzLmpvaW4oIiIpfXZhciBwYWNrZWRWZXJzaW9uVG9IdW1hblJlYWRhYmxlPW49PltuLzFlNHwwLChuLzEwMHwwKSUxMDAsbiUxMDBdLmpvaW4oIi4iKTt2YXIgVEFSR0VUX05PVF9TVVBQT1JURUQ9MjE0NzQ4MzY0Nzt2YXIgY3VycmVudE5vZGVWZXJzaW9uPXR5cGVvZiBwcm9jZXNzIT09InVuZGVmaW5lZCImJnByb2Nlc3MudmVyc2lvbnM/Lm5vZGU/aHVtYW5SZWFkYWJsZVZlcnNpb25Ub1BhY2tlZChwcm9jZXNzLnZlcnNpb25zLm5vZGUpOlRBUkdFVF9OT1RfU1VQUE9SVEVEO2lmKGN1cnJlbnROb2RlVmVyc2lvbjxUQVJHRVRfTk9UX1NVUFBPUlRFRCl7dGhyb3cgbmV3IEVycm9yKCJub3QgY29tcGlsZWQgZm9yIHRoaXMgZW52aXJvbm1lbnQgKGRpZCB5b3UgYnVpbGQgdG8gSFRNTCBhbmQgdHJ5IHRvIHJ1biBpdCBub3Qgb24gdGhlIHdlYiwgb3Igc2V0IEVOVklST05NRU5UIHRvIHNvbWV0aGluZyAtIGxpa2Ugbm9kZSAtIGFuZCBydW4gaXQgc29tZXBsYWNlIGVsc2UgLSBsaWtlIG9uIHRoZSB3ZWI/KSIpfWlmKGN1cnJlbnROb2RlVmVyc2lvbjwyMTQ3NDgzNjQ3KXt0aHJvdyBuZXcgRXJyb3IoYFRoaXMgZW1zY3JpcHRlbi1nZW5lcmF0ZWQgY29kZSByZXF1aXJlcyBub2RlIHYke3BhY2tlZFZlcnNpb25Ub0h1bWFuUmVhZGFibGUoMjE0NzQ4MzY0Nyl9IChkZXRlY3RlZCB2JHtwYWNrZWRWZXJzaW9uVG9IdW1hblJlYWRhYmxlKGN1cnJlbnROb2RlVmVyc2lvbil9KWApfXZhciB1c2VyQWdlbnQ9dHlwZW9mIG5hdmlnYXRvciE9PSJ1bmRlZmluZWQiJiZuYXZpZ2F0b3IudXNlckFnZW50O2lmKCF1c2VyQWdlbnQpe3JldHVybn12YXIgY3VycmVudFNhZmFyaVZlcnNpb249dXNlckFnZW50LmluY2x1ZGVzKCJTYWZhcmkvIikmJnVzZXJBZ2VudC5tYXRjaCgvVmVyc2lvblwvKFxkK1wuP1xkKlwuP1xkKikvKT9odW1hblJlYWRhYmxlVmVyc2lvblRvUGFja2VkKHVzZXJBZ2VudC5tYXRjaCgvVmVyc2lvblwvKFxkK1wuP1xkKlwuP1xkKikvKVsxXSk6VEFSR0VUX05PVF9TVVBQT1JURUQ7aWYoY3VycmVudFNhZmFyaVZlcnNpb248MTVlNCl7dGhyb3cgbmV3IEVycm9yKGBUaGlzIGVtc2NyaXB0ZW4tZ2VuZXJhdGVkIGNvZGUgcmVxdWlyZXMgU2FmYXJpIHYke3BhY2tlZFZlcnNpb25Ub0h1bWFuUmVhZGFibGUoMTVlNCl9IChkZXRlY3RlZCB2JHtjdXJyZW50U2FmYXJpVmVyc2lvbn0pYCl9dmFyIGN1cnJlbnRGaXJlZm94VmVyc2lvbj11c2VyQWdlbnQubWF0Y2goL0ZpcmVmb3hcLyhcZCsoPzpcLlxkKyk/KS8pP3BhcnNlRmxvYXQodXNlckFnZW50Lm1hdGNoKC9GaXJlZm94XC8oXGQrKD86XC5cZCspPykvKVsxXSk6VEFSR0VUX05PVF9TVVBQT1JURUQ7aWYoY3VycmVudEZpcmVmb3hWZXJzaW9uPDExNCl7dGhyb3cgbmV3IEVycm9yKGBUaGlzIGVtc2NyaXB0ZW4tZ2VuZXJhdGVkIGNvZGUgcmVxdWlyZXMgRmlyZWZveCB2MTE0IChkZXRlY3RlZCB2JHtjdXJyZW50RmlyZWZveFZlcnNpb259KWApfXZhciBjdXJyZW50Q2hyb21lVmVyc2lvbj11c2VyQWdlbnQubWF0Y2goL0Nocm9tZVwvKFxkKyg/OlwuXGQrKT8pLyk/cGFyc2VGbG9hdCh1c2VyQWdlbnQubWF0Y2goL0Nocm9tZVwvKFxkKyg/OlwuXGQrKT8pLylbMV0pOlRBUkdFVF9OT1RfU1VQUE9SVEVEO2lmKGN1cnJlbnRDaHJvbWVWZXJzaW9uPDg1KXt0aHJvdyBuZXcgRXJyb3IoYFRoaXMgZW1zY3JpcHRlbi1nZW5lcmF0ZWQgY29kZSByZXF1aXJlcyBDaHJvbWUgdjg1IChkZXRlY3RlZCB2JHtjdXJyZW50Q2hyb21lVmVyc2lvbn0pYCl9fSkoKTt2YXIgTW9kdWxlPW1vZHVsZUFyZzt2YXIgRU5WSVJPTk1FTlRfSVNfV0VCPSEhZ2xvYmFsVGhpcy53aW5kb3c7dmFyIEVOVklST05NRU5UX0lTX1dPUktFUj0hIWdsb2JhbFRoaXMuV29ya2VyR2xvYmFsU2NvcGU7dmFyIEVOVklST05NRU5UX0lTX05PREU9Z2xvYmFsVGhpcy5wcm9jZXNzPy52ZXJzaW9ucz8ubm9kZSYmZ2xvYmFsVGhpcy5wcm9jZXNzPy50eXBlIT0icmVuZGVyZXIiO3ZhciBFTlZJUk9OTUVOVF9JU19TSEVMTD0hRU5WSVJPTk1FTlRfSVNfV0VCJiYhRU5WSVJPTk1FTlRfSVNfTk9ERSYmIUVOVklST05NRU5UX0lTX1dPUktFUjt2YXIgRU5WSVJPTk1FTlRfSVNfUFRIUkVBRD1FTlZJUk9OTUVOVF9JU19XT1JLRVImJnNlbGYubmFtZT8uc3RhcnRzV2l0aCgiZW0tcHRocmVhZCIpO2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe2Fzc2VydCghZ2xvYmFsVGhpcy5tb2R1bGVMb2FkZWQsIm1vZHVsZSBzaG91bGQgb25seSBiZSBsb2FkZWQgb25jZSBvbiBlYWNoIHB0aHJlYWQgd29ya2VyIik7Z2xvYmFsVGhpcy5tb2R1bGVMb2FkZWQ9dHJ1ZX12YXIgYXJndW1lbnRzXz1bXTt2YXIgdGhpc1Byb2dyYW09Ii4vdGhpcy5wcm9ncmFtIjt2YXIgcXVpdF89KHN0YXR1cyx0b1Rocm93KT0+e3Rocm93IHRvVGhyb3d9O3ZhciBfc2NyaXB0TmFtZT1pbXBvcnQubWV0YS51cmw7dmFyIHNjcmlwdERpcmVjdG9yeT0iIjtmdW5jdGlvbiBsb2NhdGVGaWxlKHBhdGgpe2lmKE1vZHVsZVsibG9jYXRlRmlsZSJdKXtyZXR1cm4gTW9kdWxlWyJsb2NhdGVGaWxlIl0ocGF0aCxzY3JpcHREaXJlY3RvcnkpfXJldHVybiBzY3JpcHREaXJlY3RvcnkrcGF0aH12YXIgcmVhZEFzeW5jLHJlYWRCaW5hcnk7aWYoRU5WSVJPTk1FTlRfSVNfU0hFTEwpe31lbHNlIGlmKEVOVklST05NRU5UX0lTX1dFQnx8RU5WSVJPTk1FTlRfSVNfV09SS0VSKXt0cnl7c2NyaXB0RGlyZWN0b3J5PW5ldyBVUkwoIi4iLF9zY3JpcHROYW1lKS5ocmVmfWNhdGNoe31pZighKGdsb2JhbFRoaXMud2luZG93fHxnbG9iYWxUaGlzLldvcmtlckdsb2JhbFNjb3BlKSl0aHJvdyBuZXcgRXJyb3IoIm5vdCBjb21waWxlZCBmb3IgdGhpcyBlbnZpcm9ubWVudCAoZGlkIHlvdSBidWlsZCB0byBIVE1MIGFuZCB0cnkgdG8gcnVuIGl0IG5vdCBvbiB0aGUgd2ViLCBvciBzZXQgRU5WSVJPTk1FTlQgdG8gc29tZXRoaW5nIC0gbGlrZSBub2RlIC0gYW5kIHJ1biBpdCBzb21lcGxhY2UgZWxzZSAtIGxpa2Ugb24gdGhlIHdlYj8pIik7e2lmKEVOVklST05NRU5UX0lTX1dPUktFUil7cmVhZEJpbmFyeT11cmw9Pnt2YXIgeGhyPW5ldyBYTUxIdHRwUmVxdWVzdDt4aHIub3BlbigiR0VUIix1cmwsZmFsc2UpO3hoci5yZXNwb25zZVR5cGU9ImFycmF5YnVmZmVyIjt4aHIuc2VuZChudWxsKTtyZXR1cm4gbmV3IFVpbnQ4QXJyYXkoeGhyLnJlc3BvbnNlKX19cmVhZEFzeW5jPWFzeW5jIHVybD0+e2Fzc2VydCghaXNGaWxlVVJJKHVybCksInJlYWRBc3luYyBkb2VzIG5vdCB3b3JrIHdpdGggZmlsZTovLyBVUkxzIik7dmFyIHJlc3BvbnNlPWF3YWl0IGZldGNoKHVybCx7Y3JlZGVudGlhbHM6InNhbWUtb3JpZ2luIn0pO2lmKHJlc3BvbnNlLm9rKXtyZXR1cm4gcmVzcG9uc2UuYXJyYXlCdWZmZXIoKX10aHJvdyBuZXcgRXJyb3IocmVzcG9uc2Uuc3RhdHVzKyIgOiAiK3Jlc3BvbnNlLnVybCl9fX1lbHNle3Rocm93IG5ldyBFcnJvcigiZW52aXJvbm1lbnQgZGV0ZWN0aW9uIGVycm9yIil9dmFyIG91dD1jb25zb2xlLmxvZy5iaW5kKGNvbnNvbGUpO3ZhciBlcnI9Y29uc29sZS5lcnJvci5iaW5kKGNvbnNvbGUpO2Fzc2VydChFTlZJUk9OTUVOVF9JU19XRUJ8fEVOVklST05NRU5UX0lTX1dPUktFUnx8RU5WSVJPTk1FTlRfSVNfTk9ERSwiUHRocmVhZHMgZG8gbm90IHdvcmsgaW4gdGhpcyBlbnZpcm9ubWVudCB5ZXQgKG5lZWQgV2ViIFdvcmtlcnMsIG9yIGFuIGFsdGVybmF0aXZlIHRvIHRoZW0pIik7YXNzZXJ0KCFFTlZJUk9OTUVOVF9JU19OT0RFLCJub2RlIGVudmlyb25tZW50IGRldGVjdGVkIGJ1dCBub3QgZW5hYmxlZCBhdCBidWlsZCB0aW1lLiAgQWRkIGBub2RlYCB0byBgLXNFTlZJUk9OTUVOVGAgdG8gZW5hYmxlLiIpO2Fzc2VydCghRU5WSVJPTk1FTlRfSVNfU0hFTEwsInNoZWxsIGVudmlyb25tZW50IGRldGVjdGVkIGJ1dCBub3QgZW5hYmxlZCBhdCBidWlsZCB0aW1lLiAgQWRkIGBzaGVsbGAgdG8gYC1zRU5WSVJPTk1FTlRgIHRvIGVuYWJsZS4iKTt2YXIgd2FzbUJpbmFyeTtpZighZ2xvYmFsVGhpcy5XZWJBc3NlbWJseSl7ZXJyKCJubyBuYXRpdmUgd2FzbSBzdXBwb3J0IGRldGVjdGVkIil9dmFyIHdhc21Nb2R1bGU7dmFyIEFCT1JUPWZhbHNlO3ZhciBFWElUU1RBVFVTO2Z1bmN0aW9uIGFzc2VydChjb25kaXRpb24sdGV4dCl7aWYoIWNvbmRpdGlvbil7YWJvcnQoIkFzc2VydGlvbiBmYWlsZWQiKyh0ZXh0PyI6ICIrdGV4dDoiIikpfX12YXIgaXNGaWxlVVJJPWZpbGVuYW1lPT5maWxlbmFtZS5zdGFydHNXaXRoKCJmaWxlOi8vIik7ZnVuY3Rpb24gd3JpdGVTdGFja0Nvb2tpZSgpe3ZhciBtYXg9X2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZCgpO2Fzc2VydCgobWF4JjMpPT0wKTtpZihtYXg9PTApe21heCs9NH0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4Pj4yXT0zNDgyMTIyMzsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4KzQ+PjJdPTIzMTA3MjEwMjI7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpWzA+PjJdPTE2Njg1MDkwMjl9ZnVuY3Rpb24gY2hlY2tTdGFja0Nvb2tpZSgpe2lmKEFCT1JUKXJldHVybjt2YXIgbWF4PV9lbXNjcmlwdGVuX3N0YWNrX2dldF9lbmQoKTtpZihtYXg9PTApe21heCs9NH12YXIgY29va2llMT0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4Pj4yXTt2YXIgY29va2llMj0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbbWF4KzQ+PjJdO2lmKGNvb2tpZTEhPTM0ODIxMjIzfHxjb29raWUyIT0yMzEwNzIxMDIyKXthYm9ydChgU3RhY2sgb3ZlcmZsb3chIFN0YWNrIGNvb2tpZSBoYXMgYmVlbiBvdmVyd3JpdHRlbiBhdCAke3B0clRvU3RyaW5nKG1heCl9LCBleHBlY3RlZCBoZXggZHdvcmRzIDB4ODlCQUNERkUgYW5kIDB4MjEzNTQ2NywgYnV0IHJlY2VpdmVkICR7cHRyVG9TdHJpbmcoY29va2llMil9ICR7cHRyVG9TdHJpbmcoY29va2llMSl9YCl9aWYoKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpWzA+PjJdIT0xNjY4NTA5MDI5KXthYm9ydCgiUnVudGltZSBlcnJvcjogVGhlIGFwcGxpY2F0aW9uIGhhcyBjb3JydXB0ZWQgaXRzIGhlYXAgbWVtb3J5IGFyZWEgKGFkZHJlc3MgemVybykhIil9fXZhciBydW50aW1lRGVidWc9dHJ1ZTtmdW5jdGlvbiBkYmcoLi4uYXJncyl7aWYoIXJ1bnRpbWVEZWJ1ZyYmdHlwZW9mIHJ1bnRpbWVEZWJ1ZyE9InVuZGVmaW5lZCIpcmV0dXJuO2NvbnNvbGUud2FybiguLi5hcmdzKX0oKCk9Pnt2YXIgaDE2PW5ldyBJbnQxNkFycmF5KDEpO3ZhciBoOD1uZXcgSW50OEFycmF5KGgxNi5idWZmZXIpO2gxNlswXT0yNTQ1OTtpZihoOFswXSE9PTExNXx8aDhbMV0hPT05OSlhYm9ydCgiUnVudGltZSBlcnJvcjogZXhwZWN0ZWQgdGhlIHN5c3RlbSB0byBiZSBsaXR0bGUtZW5kaWFuISAoUnVuIHdpdGggLXNTVVBQT1JUX0JJR19FTkRJQU4gdG8gYnlwYXNzKSIpfSkoKTtmdW5jdGlvbiBjb25zdW1lZE1vZHVsZVByb3AocHJvcCl7aWYoIU9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3IoTW9kdWxlLHByb3ApKXtPYmplY3QuZGVmaW5lUHJvcGVydHkoTW9kdWxlLHByb3Ase2NvbmZpZ3VyYWJsZTp0cnVlLHNldCgpe2Fib3J0KGBBdHRlbXB0IHRvIHNldCBcYE1vZHVsZS4ke3Byb3B9XGAgYWZ0ZXIgaXQgaGFzIGFscmVhZHkgYmVlbiBwcm9jZXNzZWQuICBUaGlzIGNhbiBoYXBwZW4sIGZvciBleGFtcGxlLCB3aGVuIGNvZGUgaXMgaW5qZWN0ZWQgdmlhICctLXBvc3QtanMnIHJhdGhlciB0aGFuICctLXByZS1qcydgKX19KX19ZnVuY3Rpb24gbWFrZUludmFsaWRFYXJseUFjY2VzcyhuYW1lKXtyZXR1cm4oKT0+YXNzZXJ0KGZhbHNlLGBjYWxsIHRvICcke25hbWV9JyB2aWEgcmVmZXJlbmNlIHRha2VuIGJlZm9yZSBXYXNtIG1vZHVsZSBpbml0aWFsaXphdGlvbmApfWZ1bmN0aW9uIGlnbm9yZWRNb2R1bGVQcm9wKHByb3Ape2lmKE9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3IoTW9kdWxlLHByb3ApKXthYm9ydChgXGBNb2R1bGUuJHtwcm9wfVxgIHdhcyBzdXBwbGllZCBidXQgXGAke3Byb3B9XGAgbm90IGluY2x1ZGVkIGluIElOQ09NSU5HX01PRFVMRV9KU19BUElgKX19ZnVuY3Rpb24gaXNFeHBvcnRlZEJ5Rm9yY2VGaWxlc3lzdGVtKG5hbWUpe3JldHVybiBuYW1lPT09IkZTX2NyZWF0ZVBhdGgifHxuYW1lPT09IkZTX2NyZWF0ZURhdGFGaWxlInx8bmFtZT09PSJGU19jcmVhdGVQcmVsb2FkZWRGaWxlInx8bmFtZT09PSJGU19wcmVsb2FkRmlsZSJ8fG5hbWU9PT0iRlNfdW5saW5rInx8bmFtZT09PSJhZGRSdW5EZXBlbmRlbmN5Inx8bmFtZT09PSJGU19jcmVhdGVMYXp5RmlsZSJ8fG5hbWU9PT0iRlNfY3JlYXRlRGV2aWNlInx8bmFtZT09PSJyZW1vdmVSdW5EZXBlbmRlbmN5In1mdW5jdGlvbiBtaXNzaW5nTGlicmFyeVN5bWJvbChzeW0pe3VuZXhwb3J0ZWRSdW50aW1lU3ltYm9sKHN5bSl9ZnVuY3Rpb24gdW5leHBvcnRlZFJ1bnRpbWVTeW1ib2woc3ltKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtyZXR1cm59aWYoIU9iamVjdC5nZXRPd25Qcm9wZXJ0eURlc2NyaXB0b3IoTW9kdWxlLHN5bSkpe09iamVjdC5kZWZpbmVQcm9wZXJ0eShNb2R1bGUsc3ltLHtjb25maWd1cmFibGU6dHJ1ZSxnZXQoKXt2YXIgbXNnPWAnJHtzeW19JyB3YXMgbm90IGV4cG9ydGVkLiBhZGQgaXQgdG8gRVhQT1JURURfUlVOVElNRV9NRVRIT0RTIChzZWUgdGhlIEVtc2NyaXB0ZW4gRkFRKWA7aWYoaXNFeHBvcnRlZEJ5Rm9yY2VGaWxlc3lzdGVtKHN5bSkpe21zZys9Ii4gQWx0ZXJuYXRpdmVseSwgZm9yY2luZyBmaWxlc3lzdGVtIHN1cHBvcnQgKC1zRk9SQ0VfRklMRVNZU1RFTSkgY2FuIGV4cG9ydCB0aGlzIGZvciB5b3UifWFib3J0KG1zZyl9fSl9fWZ1bmN0aW9uIGluaXRXb3JrZXJMb2dnaW5nKCl7ZnVuY3Rpb24gZ2V0TG9nUHJlZml4KCl7dmFyIHQ9MDtpZihydW50aW1lSW5pdGlhbGl6ZWQmJnR5cGVvZiBfcHRocmVhZF9zZWxmIT0idW5kZWZpbmVkIil7dD1fcHRocmVhZF9zZWxmKCl9cmV0dXJuYHc6JHt3b3JrZXJJRH0sdDoke3B0clRvU3RyaW5nKHQpfTpgfXZhciBvcmlnRGJnPWRiZztkYmc9KC4uLmFyZ3MpPT5vcmlnRGJnKGdldExvZ1ByZWZpeCgpLC4uLmFyZ3MpfWluaXRXb3JrZXJMb2dnaW5nKCk7ZnVuY3Rpb24gZ3Jvd01lbVZpZXdzKCl7aWYod2FzbU1lbW9yeS5idWZmZXIhPUhFQVA4LmJ1ZmZlcil7dXBkYXRlTWVtb3J5Vmlld3MoKX19dmFyIHJlYWR5UHJvbWlzZVJlc29sdmUscmVhZHlQcm9taXNlUmVqZWN0O3ZhciB3b3JrZXJJRD0wO3ZhciBzdGFydFdvcmtlcjtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXt2YXIgaW5pdGlhbGl6ZWRKUz1mYWxzZTtzZWxmLm9udW5oYW5kbGVkcmVqZWN0aW9uPWU9Pnt0aHJvdyBlLnJlYXNvbnx8ZX07ZnVuY3Rpb24gaGFuZGxlTWVzc2FnZShlKXt0cnl7dmFyIG1zZ0RhdGE9ZVsiZGF0YSJdO3ZhciBjbWQ9bXNnRGF0YS5jbWQ7aWYoY21kPT09ImxvYWQiKXt3b3JrZXJJRD1tc2dEYXRhLndvcmtlcklEO2xldCBtZXNzYWdlUXVldWU9W107c2VsZi5vbm1lc3NhZ2U9ZT0+bWVzc2FnZVF1ZXVlLnB1c2goZSk7c3RhcnRXb3JrZXI9KCk9Pntwb3N0TWVzc2FnZSh7Y21kOiJsb2FkZWQifSk7Zm9yKGxldCBtc2cgb2YgbWVzc2FnZVF1ZXVlKXtoYW5kbGVNZXNzYWdlKG1zZyl9c2VsZi5vbm1lc3NhZ2U9aGFuZGxlTWVzc2FnZX07Zm9yKGNvbnN0IGhhbmRsZXIgb2YgbXNnRGF0YS5oYW5kbGVycyl7aWYoIU1vZHVsZVtoYW5kbGVyXXx8TW9kdWxlW2hhbmRsZXJdLnByb3h5KXtNb2R1bGVbaGFuZGxlcl09KC4uLmFyZ3MpPT57cG9zdE1lc3NhZ2Uoe2NtZDoiY2FsbEhhbmRsZXIiLGhhbmRsZXIsYXJnc30pfTtpZihoYW5kbGVyPT0icHJpbnQiKW91dD1Nb2R1bGVbaGFuZGxlcl07aWYoaGFuZGxlcj09InByaW50RXJyIillcnI9TW9kdWxlW2hhbmRsZXJdfX13YXNtTWVtb3J5PW1zZ0RhdGEud2FzbU1lbW9yeTt1cGRhdGVNZW1vcnlWaWV3cygpO3dhc21Nb2R1bGU9bXNnRGF0YS53YXNtTW9kdWxlO2NyZWF0ZVdhc20oKTtydW4oKX1lbHNlIGlmKGNtZD09PSJydW4iKXthc3NlcnQobXNnRGF0YS5wdGhyZWFkX3B0cik7ZXN0YWJsaXNoU3RhY2tTcGFjZShtc2dEYXRhLnB0aHJlYWRfcHRyKTtfX2Vtc2NyaXB0ZW5fdGhyZWFkX2luaXQobXNnRGF0YS5wdGhyZWFkX3B0ciwwLDAsMSwwLDApO1BUaHJlYWQudGhyZWFkSW5pdFRMUygpO19fZW1zY3JpcHRlbl90aHJlYWRfbWFpbGJveF9hd2FpdChtc2dEYXRhLnB0aHJlYWRfcHRyKTtpZighaW5pdGlhbGl6ZWRKUyl7X19lbWJpbmRfaW5pdGlhbGl6ZV9iaW5kaW5ncygpO2luaXRpYWxpemVkSlM9dHJ1ZX10cnl7aW52b2tlRW50cnlQb2ludChtc2dEYXRhLnN0YXJ0X3JvdXRpbmUsbXNnRGF0YS5hcmcpfWNhdGNoKGV4KXtpZihleCE9InVud2luZCIpe3Rocm93IGV4fX19ZWxzZSBpZihtc2dEYXRhLnRhcmdldD09PSJzZXRpbW1lZGlhdGUiKXt9ZWxzZSBpZihjbWQ9PT0iY2hlY2tNYWlsYm94Iil7aWYoaW5pdGlhbGl6ZWRKUyl7Y2hlY2tNYWlsYm94KCl9fWVsc2UgaWYoY21kKXtlcnIoYHdvcmtlcjogcmVjZWl2ZWQgdW5rbm93biBjb21tYW5kICR7Y21kfWApO2Vycihtc2dEYXRhKX19Y2F0Y2goZXgpe2Vycihgd29ya2VyOiBvbm1lc3NhZ2UoKSBjYXB0dXJlZCBhbiB1bmNhdWdodCBleGNlcHRpb246ICR7ZXh9YCk7aWYoZXg/LnN0YWNrKWVycihleC5zdGFjayk7X19lbXNjcmlwdGVuX3RocmVhZF9jcmFzaGVkKCk7dGhyb3cgZXh9fXNlbGYub25tZXNzYWdlPWhhbmRsZU1lc3NhZ2V9dmFyIEhFQVA4LEhFQVBVOCxIRUFQMTYsSEVBUFUxNixIRUFQMzIsSEVBUFUzMixIRUFQRjMyLEhFQVBGNjQ7dmFyIEhFQVA2NCxIRUFQVTY0O3ZhciBydW50aW1lSW5pdGlhbGl6ZWQ9ZmFsc2U7ZnVuY3Rpb24gdXBkYXRlTWVtb3J5Vmlld3MoKXt2YXIgYj13YXNtTWVtb3J5LmJ1ZmZlcjtIRUFQOD1uZXcgSW50OEFycmF5KGIpO0hFQVAxNj1uZXcgSW50MTZBcnJheShiKTtNb2R1bGVbIkhFQVBVOCJdPUhFQVBVOD1uZXcgVWludDhBcnJheShiKTtIRUFQVTE2PW5ldyBVaW50MTZBcnJheShiKTtIRUFQMzI9bmV3IEludDMyQXJyYXkoYik7SEVBUFUzMj1uZXcgVWludDMyQXJyYXkoYik7SEVBUEYzMj1uZXcgRmxvYXQzMkFycmF5KGIpO0hFQVBGNjQ9bmV3IEZsb2F0NjRBcnJheShiKTtIRUFQNjQ9bmV3IEJpZ0ludDY0QXJyYXkoYik7SEVBUFU2ND1uZXcgQmlnVWludDY0QXJyYXkoYil9ZnVuY3Rpb24gaW5pdE1lbW9yeSgpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3JldHVybn1pZihNb2R1bGVbIndhc21NZW1vcnkiXSl7d2FzbU1lbW9yeT1Nb2R1bGVbIndhc21NZW1vcnkiXX1lbHNle3ZhciBJTklUSUFMX01FTU9SWT1Nb2R1bGVbIklOSVRJQUxfTUVNT1JZIl18fDE2Nzc3MjE2O2Fzc2VydChJTklUSUFMX01FTU9SWT49NjU1MzYsIklOSVRJQUxfTUVNT1JZIHNob3VsZCBiZSBsYXJnZXIgdGhhbiBTVEFDS19TSVpFLCB3YXMgIitJTklUSUFMX01FTU9SWSsiISAoU1RBQ0tfU0laRT0iKzY1NTM2KyIpIik7d2FzbU1lbW9yeT1uZXcgV2ViQXNzZW1ibHkuTWVtb3J5KHtpbml0aWFsOklOSVRJQUxfTUVNT1JZLzY1NTM2LG1heGltdW06MzI3Njgsc2hhcmVkOnRydWV9KX11cGRhdGVNZW1vcnlWaWV3cygpfWFzc2VydChnbG9iYWxUaGlzLkludDMyQXJyYXkmJmdsb2JhbFRoaXMuRmxvYXQ2NEFycmF5JiZJbnQzMkFycmF5LnByb3RvdHlwZS5zdWJhcnJheSYmSW50MzJBcnJheS5wcm90b3R5cGUuc2V0LCJKUyBlbmdpbmUgZG9lcyBub3QgcHJvdmlkZSBmdWxsIHR5cGVkIGFycmF5IHN1cHBvcnQiKTtmdW5jdGlvbiBwcmVSdW4oKXthc3NlcnQoIUVOVklST05NRU5UX0lTX1BUSFJFQUQpO2lmKE1vZHVsZVsicHJlUnVuIl0pe2lmKHR5cGVvZiBNb2R1bGVbInByZVJ1biJdPT0iZnVuY3Rpb24iKU1vZHVsZVsicHJlUnVuIl09W01vZHVsZVsicHJlUnVuIl1dO3doaWxlKE1vZHVsZVsicHJlUnVuIl0ubGVuZ3RoKXthZGRPblByZVJ1bihNb2R1bGVbInByZVJ1biJdLnNoaWZ0KCkpfX1jb25zdW1lZE1vZHVsZVByb3AoInByZVJ1biIpO2NhbGxSdW50aW1lQ2FsbGJhY2tzKG9uUHJlUnVucyl9ZnVuY3Rpb24gaW5pdFJ1bnRpbWUoKXthc3NlcnQoIXJ1bnRpbWVJbml0aWFsaXplZCk7cnVudGltZUluaXRpYWxpemVkPXRydWU7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gc3RhcnRXb3JrZXIoKTtjaGVja1N0YWNrQ29va2llKCk7aWYoIU1vZHVsZVsibm9GU0luaXQiXSYmIUZTLmluaXRpYWxpemVkKUZTLmluaXQoKTtUVFkuaW5pdCgpO3dhc21FeHBvcnRzWyJfX3dhc21fY2FsbF9jdG9ycyJdKCk7RlMuaWdub3JlUGVybWlzc2lvbnM9ZmFsc2V9ZnVuY3Rpb24gcG9zdFJ1bigpe2NoZWNrU3RhY2tDb29raWUoKTtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtyZXR1cm59aWYoTW9kdWxlWyJwb3N0UnVuIl0pe2lmKHR5cGVvZiBNb2R1bGVbInBvc3RSdW4iXT09ImZ1bmN0aW9uIilNb2R1bGVbInBvc3RSdW4iXT1bTW9kdWxlWyJwb3N0UnVuIl1dO3doaWxlKE1vZHVsZVsicG9zdFJ1biJdLmxlbmd0aCl7YWRkT25Qb3N0UnVuKE1vZHVsZVsicG9zdFJ1biJdLnNoaWZ0KCkpfX1jb25zdW1lZE1vZHVsZVByb3AoInBvc3RSdW4iKTtjYWxsUnVudGltZUNhbGxiYWNrcyhvblBvc3RSdW5zKX1mdW5jdGlvbiBhYm9ydCh3aGF0KXtNb2R1bGVbIm9uQWJvcnQiXT8uKHdoYXQpO3doYXQ9IkFib3J0ZWQoIit3aGF0KyIpIjtlcnIod2hhdCk7QUJPUlQ9dHJ1ZTtpZih3aGF0LmluZGV4T2YoIlJ1bnRpbWVFcnJvcjogdW5yZWFjaGFibGUiKT49MCl7d2hhdCs9Jy4gInVucmVhY2hhYmxlIiBtYXkgYmUgZHVlIHRvIEFTWU5DSUZZX1NUQUNLX1NJWkUgbm90IGJlaW5nIGxhcmdlIGVub3VnaCAodHJ5IGluY3JlYXNpbmcgaXQpJ312YXIgZT1uZXcgV2ViQXNzZW1ibHkuUnVudGltZUVycm9yKHdoYXQpO3JlYWR5UHJvbWlzZVJlamVjdD8uKGUpO3Rocm93IGV9ZnVuY3Rpb24gY3JlYXRlRXhwb3J0V3JhcHBlcihuYW1lLG5hcmdzKXtyZXR1cm4oLi4uYXJncyk9Pnthc3NlcnQocnVudGltZUluaXRpYWxpemVkLGBuYXRpdmUgZnVuY3Rpb24gXGAke25hbWV9XGAgY2FsbGVkIGJlZm9yZSBydW50aW1lIGluaXRpYWxpemF0aW9uYCk7dmFyIGY9d2FzbUV4cG9ydHNbbmFtZV07YXNzZXJ0KGYsYGV4cG9ydGVkIG5hdGl2ZSBmdW5jdGlvbiBcYCR7bmFtZX1cYCBub3QgZm91bmRgKTthc3NlcnQoYXJncy5sZW5ndGg8PW5hcmdzLGBuYXRpdmUgZnVuY3Rpb24gXGAke25hbWV9XGAgY2FsbGVkIHdpdGggJHthcmdzLmxlbmd0aH0gYXJncyBidXQgZXhwZWN0cyAke25hcmdzfWApO3JldHVybiBmKC4uLmFyZ3MpfX12YXIgd2FzbUJpbmFyeUZpbGU7ZnVuY3Rpb24gZmluZFdhc21CaW5hcnkoKXtpZihNb2R1bGVbImxvY2F0ZUZpbGUiXSl7cmV0dXJuIGxvY2F0ZUZpbGUoImNwX3NhdF9ydW50aW1lX2FzeW5jaWZ5Lndhc20iKX1yZXR1cm4gbmV3IFVSTCgiY3Bfc2F0X3J1bnRpbWVfYXN5bmNpZnkud2FzbSIsaW1wb3J0Lm1ldGEudXJsKS5ocmVmfWZ1bmN0aW9uIGdldEJpbmFyeVN5bmMoZmlsZSl7aWYoZmlsZT09d2FzbUJpbmFyeUZpbGUmJndhc21CaW5hcnkpe3JldHVybiBuZXcgVWludDhBcnJheSh3YXNtQmluYXJ5KX1pZihyZWFkQmluYXJ5KXtyZXR1cm4gcmVhZEJpbmFyeShmaWxlKX10aHJvdyJib3RoIGFzeW5jIGFuZCBzeW5jIGZldGNoaW5nIG9mIHRoZSB3YXNtIGZhaWxlZCJ9YXN5bmMgZnVuY3Rpb24gZ2V0V2FzbUJpbmFyeShiaW5hcnlGaWxlKXtpZighd2FzbUJpbmFyeSl7dHJ5e3ZhciByZXNwb25zZT1hd2FpdCByZWFkQXN5bmMoYmluYXJ5RmlsZSk7cmV0dXJuIG5ldyBVaW50OEFycmF5KHJlc3BvbnNlKX1jYXRjaHt9fXJldHVybiBnZXRCaW5hcnlTeW5jKGJpbmFyeUZpbGUpfWFzeW5jIGZ1bmN0aW9uIGluc3RhbnRpYXRlQXJyYXlCdWZmZXIoYmluYXJ5RmlsZSxpbXBvcnRzKXt0cnl7dmFyIGJpbmFyeT1hd2FpdCBnZXRXYXNtQmluYXJ5KGJpbmFyeUZpbGUpO3ZhciBpbnN0YW5jZT1hd2FpdCBXZWJBc3NlbWJseS5pbnN0YW50aWF0ZShiaW5hcnksaW1wb3J0cyk7cmV0dXJuIGluc3RhbmNlfWNhdGNoKHJlYXNvbil7ZXJyKGBmYWlsZWQgdG8gYXN5bmNocm9ub3VzbHkgcHJlcGFyZSB3YXNtOiAke3JlYXNvbn1gKTtpZihpc0ZpbGVVUkkoYmluYXJ5RmlsZSkpe2Vycihgd2FybmluZzogTG9hZGluZyBmcm9tIGEgZmlsZSBVUkkgKCR7YmluYXJ5RmlsZX0pIGlzIG5vdCBzdXBwb3J0ZWQgaW4gbW9zdCBicm93c2Vycy4gU2VlIGh0dHBzOi8vZW1zY3JpcHRlbi5vcmcvZG9jcy9nZXR0aW5nX3N0YXJ0ZWQvRkFRLmh0bWwjaG93LWRvLWktcnVuLWEtbG9jYWwtd2Vic2VydmVyLWZvci10ZXN0aW5nLXdoeS1kb2VzLW15LXByb2dyYW0tc3RhbGwtaW4tZG93bmxvYWRpbmctb3ItcHJlcGFyaW5nYCl9YWJvcnQocmVhc29uKX19YXN5bmMgZnVuY3Rpb24gaW5zdGFudGlhdGVBc3luYyhiaW5hcnksYmluYXJ5RmlsZSxpbXBvcnRzKXtpZighYmluYXJ5KXt0cnl7dmFyIHJlc3BvbnNlPWZldGNoKGJpbmFyeUZpbGUse2NyZWRlbnRpYWxzOiJzYW1lLW9yaWdpbiJ9KTt2YXIgaW5zdGFudGlhdGlvblJlc3VsdD1hd2FpdCBXZWJBc3NlbWJseS5pbnN0YW50aWF0ZVN0cmVhbWluZyhyZXNwb25zZSxpbXBvcnRzKTtyZXR1cm4gaW5zdGFudGlhdGlvblJlc3VsdH1jYXRjaChyZWFzb24pe2Vycihgd2FzbSBzdHJlYW1pbmcgY29tcGlsZSBmYWlsZWQ6ICR7cmVhc29ufWApO2VycigiZmFsbGluZyBiYWNrIHRvIEFycmF5QnVmZmVyIGluc3RhbnRpYXRpb24iKX19cmV0dXJuIGluc3RhbnRpYXRlQXJyYXlCdWZmZXIoYmluYXJ5RmlsZSxpbXBvcnRzKX1mdW5jdGlvbiBnZXRXYXNtSW1wb3J0cygpe2Fzc2lnbldhc21JbXBvcnRzKCk7aWYoIXdhc21JbXBvcnRzLl9faW5zdHJ1bWVudGVkKXt3YXNtSW1wb3J0cy5fX2luc3RydW1lbnRlZD10cnVlO0FzeW5jaWZ5Lmluc3RydW1lbnRXYXNtSW1wb3J0cyh3YXNtSW1wb3J0cyl9dmFyIGltcG9ydHM9e2Vudjp3YXNtSW1wb3J0cyx3YXNpX3NuYXBzaG90X3ByZXZpZXcxOndhc21JbXBvcnRzfTtyZXR1cm4gaW1wb3J0c31hc3luYyBmdW5jdGlvbiBjcmVhdGVXYXNtKCl7ZnVuY3Rpb24gcmVjZWl2ZUluc3RhbmNlKGluc3RhbmNlLG1vZHVsZSl7d2FzbUV4cG9ydHM9aW5zdGFuY2UuZXhwb3J0czt3YXNtRXhwb3J0cz1Bc3luY2lmeS5pbnN0cnVtZW50V2FzbUV4cG9ydHMod2FzbUV4cG9ydHMpO3JlZ2lzdGVyVExTSW5pdCh3YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fdGxzX2luaXQiXSk7YXNzaWduV2FzbUV4cG9ydHMod2FzbUV4cG9ydHMpO3dhc21Nb2R1bGU9bW9kdWxlO3JldHVybiB3YXNtRXhwb3J0c312YXIgdHJ1ZU1vZHVsZT1Nb2R1bGU7ZnVuY3Rpb24gcmVjZWl2ZUluc3RhbnRpYXRpb25SZXN1bHQocmVzdWx0KXthc3NlcnQoTW9kdWxlPT09dHJ1ZU1vZHVsZSwidGhlIE1vZHVsZSBvYmplY3Qgc2hvdWxkIG5vdCBiZSByZXBsYWNlZCBkdXJpbmcgYXN5bmMgY29tcGlsYXRpb24gLSBwZXJoYXBzIHRoZSBvcmRlciBvZiBIVE1MIGVsZW1lbnRzIGlzIHdyb25nPyIpO3RydWVNb2R1bGU9bnVsbDtyZXR1cm4gcmVjZWl2ZUluc3RhbmNlKHJlc3VsdFsiaW5zdGFuY2UiXSxyZXN1bHRbIm1vZHVsZSJdKX12YXIgaW5mbz1nZXRXYXNtSW1wb3J0cygpO2lmKE1vZHVsZVsiaW5zdGFudGlhdGVXYXNtIl0pe3JldHVybiBuZXcgUHJvbWlzZSgocmVzb2x2ZSxyZWplY3QpPT57dHJ5e01vZHVsZVsiaW5zdGFudGlhdGVXYXNtIl0oaW5mbywoaW5zdCxtb2QpPT57cmVzb2x2ZShyZWNlaXZlSW5zdGFuY2UoaW5zdCxtb2QpKX0pfWNhdGNoKGUpe2VycihgTW9kdWxlLmluc3RhbnRpYXRlV2FzbSBjYWxsYmFjayBmYWlsZWQgd2l0aCBlcnJvcjogJHtlfWApO3JlamVjdChlKX19KX1pZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXthc3NlcnQod2FzbU1vZHVsZSwid2FzbU1vZHVsZSBzaG91bGQgaGF2ZSBiZWVuIHJlY2VpdmVkIHZpYSBwb3N0TWVzc2FnZSIpO3ZhciBpbnN0YW5jZT1uZXcgV2ViQXNzZW1ibHkuSW5zdGFuY2Uod2FzbU1vZHVsZSxnZXRXYXNtSW1wb3J0cygpKTtyZXR1cm4gcmVjZWl2ZUluc3RhbmNlKGluc3RhbmNlLHdhc21Nb2R1bGUpfXdhc21CaW5hcnlGaWxlPz89ZmluZFdhc21CaW5hcnkoKTt2YXIgcmVzdWx0PWF3YWl0IGluc3RhbnRpYXRlQXN5bmMod2FzbUJpbmFyeSx3YXNtQmluYXJ5RmlsZSxpbmZvKTt2YXIgZXhwb3J0cz1yZWNlaXZlSW5zdGFudGlhdGlvblJlc3VsdChyZXN1bHQpO3JldHVybiBleHBvcnRzfWNsYXNzIEV4aXRTdGF0dXN7bmFtZT0iRXhpdFN0YXR1cyI7Y29uc3RydWN0b3Ioc3RhdHVzKXt0aGlzLm1lc3NhZ2U9YFByb2dyYW0gdGVybWluYXRlZCB3aXRoIGV4aXQoJHtzdGF0dXN9KWA7dGhpcy5zdGF0dXM9c3RhdHVzfX12YXIgdGVybWluYXRlV29ya2VyPXdvcmtlcj0+e3dvcmtlci50ZXJtaW5hdGUoKTt3b3JrZXIub25tZXNzYWdlPWU9Pnt2YXIgY21kPWVbImRhdGEiXS5jbWQ7ZXJyKGByZWNlaXZlZCAiJHtjbWR9IiBjb21tYW5kIGZyb20gdGVybWluYXRlZCB3b3JrZXI6ICR7d29ya2VyLndvcmtlcklEfWApfX07dmFyIGNsZWFudXBUaHJlYWQ9cHRocmVhZF9wdHI9Pnthc3NlcnQoIUVOVklST05NRU5UX0lTX1BUSFJFQUQsIkludGVybmFsIEVycm9yISBjbGVhbnVwVGhyZWFkKCkgY2FuIG9ubHkgZXZlciBiZSBjYWxsZWQgZnJvbSBtYWluIGFwcGxpY2F0aW9uIHRocmVhZCEiKTthc3NlcnQocHRocmVhZF9wdHIsIkludGVybmFsIEVycm9yISBOdWxsIHB0aHJlYWRfcHRyIGluIGNsZWFudXBUaHJlYWQhIik7dmFyIHdvcmtlcj1QVGhyZWFkLnB0aHJlYWRzW3B0aHJlYWRfcHRyXTthc3NlcnQod29ya2VyKTtQVGhyZWFkLnJldHVybldvcmtlclRvUG9vbCh3b3JrZXIpfTt2YXIgY2FsbFJ1bnRpbWVDYWxsYmFja3M9Y2FsbGJhY2tzPT57d2hpbGUoY2FsbGJhY2tzLmxlbmd0aD4wKXtjYWxsYmFja3Muc2hpZnQoKShNb2R1bGUpfX07dmFyIG9uUHJlUnVucz1bXTt2YXIgYWRkT25QcmVSdW49Y2I9Pm9uUHJlUnVucy5wdXNoKGNiKTt2YXIgcnVuRGVwZW5kZW5jaWVzPTA7dmFyIGRlcGVuZGVuY2llc0Z1bGZpbGxlZD1udWxsO3ZhciBydW5EZXBlbmRlbmN5VHJhY2tpbmc9e307dmFyIHJ1bkRlcGVuZGVuY3lXYXRjaGVyPW51bGw7dmFyIHJlbW92ZVJ1bkRlcGVuZGVuY3k9aWQ9PntydW5EZXBlbmRlbmNpZXMtLTtNb2R1bGVbIm1vbml0b3JSdW5EZXBlbmRlbmNpZXMiXT8uKHJ1bkRlcGVuZGVuY2llcyk7YXNzZXJ0KGlkLCJyZW1vdmVSdW5EZXBlbmRlbmN5IHJlcXVpcmVzIGFuIElEIik7YXNzZXJ0KHJ1bkRlcGVuZGVuY3lUcmFja2luZ1tpZF0pO2RlbGV0ZSBydW5EZXBlbmRlbmN5VHJhY2tpbmdbaWRdO2lmKHJ1bkRlcGVuZGVuY2llcz09MCl7aWYocnVuRGVwZW5kZW5jeVdhdGNoZXIhPT1udWxsKXtjbGVhckludGVydmFsKHJ1bkRlcGVuZGVuY3lXYXRjaGVyKTtydW5EZXBlbmRlbmN5V2F0Y2hlcj1udWxsfWlmKGRlcGVuZGVuY2llc0Z1bGZpbGxlZCl7dmFyIGNhbGxiYWNrPWRlcGVuZGVuY2llc0Z1bGZpbGxlZDtkZXBlbmRlbmNpZXNGdWxmaWxsZWQ9bnVsbDtjYWxsYmFjaygpfX19O3ZhciBhZGRSdW5EZXBlbmRlbmN5PWlkPT57cnVuRGVwZW5kZW5jaWVzKys7TW9kdWxlWyJtb25pdG9yUnVuRGVwZW5kZW5jaWVzIl0/LihydW5EZXBlbmRlbmNpZXMpO2Fzc2VydChpZCwiYWRkUnVuRGVwZW5kZW5jeSByZXF1aXJlcyBhbiBJRCIpO2Fzc2VydCghcnVuRGVwZW5kZW5jeVRyYWNraW5nW2lkXSk7cnVuRGVwZW5kZW5jeVRyYWNraW5nW2lkXT0xO2lmKHJ1bkRlcGVuZGVuY3lXYXRjaGVyPT09bnVsbCYmZ2xvYmFsVGhpcy5zZXRJbnRlcnZhbCl7cnVuRGVwZW5kZW5jeVdhdGNoZXI9c2V0SW50ZXJ2YWwoKCk9PntpZihBQk9SVCl7Y2xlYXJJbnRlcnZhbChydW5EZXBlbmRlbmN5V2F0Y2hlcik7cnVuRGVwZW5kZW5jeVdhdGNoZXI9bnVsbDtyZXR1cm59dmFyIHNob3duPWZhbHNlO2Zvcih2YXIgZGVwIGluIHJ1bkRlcGVuZGVuY3lUcmFja2luZyl7aWYoIXNob3duKXtzaG93bj10cnVlO2Vycigic3RpbGwgd2FpdGluZyBvbiBydW4gZGVwZW5kZW5jaWVzOiIpfWVycihgZGVwZW5kZW5jeTogJHtkZXB9YCl9aWYoc2hvd24pe2VycigiKGVuZCBvZiBsaXN0KSIpfX0sMWU0KX19O3ZhciBzcGF3blRocmVhZD10aHJlYWRQYXJhbXM9Pnthc3NlcnQoIUVOVklST05NRU5UX0lTX1BUSFJFQUQsIkludGVybmFsIEVycm9yISBzcGF3blRocmVhZCgpIGNhbiBvbmx5IGV2ZXIgYmUgY2FsbGVkIGZyb20gbWFpbiBhcHBsaWNhdGlvbiB0aHJlYWQhIik7YXNzZXJ0KHRocmVhZFBhcmFtcy5wdGhyZWFkX3B0ciwiSW50ZXJuYWwgZXJyb3IsIG5vIHB0aHJlYWQgcHRyISIpO3ZhciB3b3JrZXI9UFRocmVhZC5nZXROZXdXb3JrZXIoKTtpZighd29ya2VyKXtyZXR1cm4gNn1hc3NlcnQoIXdvcmtlci5wdGhyZWFkX3B0ciwiSW50ZXJuYWwgZXJyb3IhIik7UFRocmVhZC5ydW5uaW5nV29ya2Vycy5wdXNoKHdvcmtlcik7UFRocmVhZC5wdGhyZWFkc1t0aHJlYWRQYXJhbXMucHRocmVhZF9wdHJdPXdvcmtlcjt3b3JrZXIucHRocmVhZF9wdHI9dGhyZWFkUGFyYW1zLnB0aHJlYWRfcHRyO3ZhciBtc2c9e2NtZDoicnVuIixzdGFydF9yb3V0aW5lOnRocmVhZFBhcmFtcy5zdGFydFJvdXRpbmUsYXJnOnRocmVhZFBhcmFtcy5hcmcscHRocmVhZF9wdHI6dGhyZWFkUGFyYW1zLnB0aHJlYWRfcHRyfTt3b3JrZXIucG9zdE1lc3NhZ2UobXNnLHRocmVhZFBhcmFtcy50cmFuc2Zlckxpc3QpO3JldHVybiAwfTt2YXIgcnVudGltZUtlZXBhbGl2ZUNvdW50ZXI9MDt2YXIga2VlcFJ1bnRpbWVBbGl2ZT0oKT0+bm9FeGl0UnVudGltZXx8cnVudGltZUtlZXBhbGl2ZUNvdW50ZXI+MDt2YXIgc3RhY2tTYXZlPSgpPT5fZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudCgpO3ZhciBzdGFja1Jlc3RvcmU9dmFsPT5fX2Vtc2NyaXB0ZW5fc3RhY2tfcmVzdG9yZSh2YWwpO3ZhciBzdGFja0FsbG9jPXN6PT5fX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2Moc3opO3ZhciBwcm94eVRvTWFpblRocmVhZD0oZnVuY0luZGV4LGVtQXNtQWRkcixzeW5jLC4uLmNhbGxBcmdzKT0+e3ZhciBzZXJpYWxpemVkTnVtQ2FsbEFyZ3M9Y2FsbEFyZ3MubGVuZ3RoKjI7dmFyIHNwPXN0YWNrU2F2ZSgpO3ZhciBhcmdzPXN0YWNrQWxsb2Moc2VyaWFsaXplZE51bUNhbGxBcmdzKjgpO3ZhciBiPWFyZ3M+PjM7Zm9yKHZhciBpPTA7aTxjYWxsQXJncy5sZW5ndGg7aSsrKXt2YXIgYXJnPWNhbGxBcmdzW2ldO2lmKHR5cGVvZiBhcmc9PSJiaWdpbnQiKXsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtiKzIqaV09MW47KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYisyKmkrMV09YXJnfWVsc2V7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYisyKmldPTBuOyhncm93TWVtVmlld3MoKSxIRUFQRjY0KVtiKzIqaSsxXT1hcmd9fXZhciBydG49X19lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZChmdW5jSW5kZXgsZW1Bc21BZGRyLHNlcmlhbGl6ZWROdW1DYWxsQXJncyxhcmdzLHN5bmMpO3N0YWNrUmVzdG9yZShzcCk7cmV0dXJuIHJ0bn07ZnVuY3Rpb24gX3Byb2NfZXhpdChjb2RlKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgwLDAsMSxjb2RlKTtFWElUU1RBVFVTPWNvZGU7aWYoIWtlZXBSdW50aW1lQWxpdmUoKSl7UFRocmVhZC50ZXJtaW5hdGVBbGxUaHJlYWRzKCk7TW9kdWxlWyJvbkV4aXQiXT8uKGNvZGUpO0FCT1JUPXRydWV9cXVpdF8oY29kZSxuZXcgRXhpdFN0YXR1cyhjb2RlKSl9ZnVuY3Rpb24gZXhpdE9uTWFpblRocmVhZChyZXR1cm5Db2RlKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxLDAsMCxyZXR1cm5Db2RlKTtfZXhpdChyZXR1cm5Db2RlKX12YXIgZXhpdEpTPShzdGF0dXMsaW1wbGljaXQpPT57RVhJVFNUQVRVUz1zdGF0dXM7Y2hlY2tVbmZsdXNoZWRDb250ZW50KCk7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCl7YXNzZXJ0KCFpbXBsaWNpdCk7ZXhpdE9uTWFpblRocmVhZChzdGF0dXMpO3Rocm93InVud2luZCJ9aWYoa2VlcFJ1bnRpbWVBbGl2ZSgpJiYhaW1wbGljaXQpe3ZhciBtc2c9YHByb2dyYW0gZXhpdGVkICh3aXRoIHN0YXR1czogJHtzdGF0dXN9KSwgYnV0IGtlZXBSdW50aW1lQWxpdmUoKSBpcyBzZXQgKGNvdW50ZXI9JHtydW50aW1lS2VlcGFsaXZlQ291bnRlcn0pIGR1ZSB0byBhbiBhc3luYyBvcGVyYXRpb24sIHNvIGhhbHRpbmcgZXhlY3V0aW9uIGJ1dCBub3QgZXhpdGluZyB0aGUgcnVudGltZSBvciBwcmV2ZW50aW5nIGZ1cnRoZXIgYXN5bmMgZXhlY3V0aW9uICh5b3UgY2FuIHVzZSBlbXNjcmlwdGVuX2ZvcmNlX2V4aXQsIGlmIHlvdSB3YW50IHRvIGZvcmNlIGEgdHJ1ZSBzaHV0ZG93bilgO3JlYWR5UHJvbWlzZVJlamVjdD8uKG1zZyk7ZXJyKG1zZyl9X3Byb2NfZXhpdChzdGF0dXMpfTt2YXIgX2V4aXQ9ZXhpdEpTO3ZhciBwdHJUb1N0cmluZz1wdHI9Pnthc3NlcnQodHlwZW9mIHB0cj09PSJudW1iZXIiLGBwdHJUb1N0cmluZyBleHBlY3RzIGEgbnVtYmVyLCBnb3QgJHt0eXBlb2YgcHRyfWApO3B0cj4+Pj0wO3JldHVybiIweCIrcHRyLnRvU3RyaW5nKDE2KS5wYWRTdGFydCg4LCIwIil9O3ZhciBQVGhyZWFkPXt1bnVzZWRXb3JrZXJzOltdLHJ1bm5pbmdXb3JrZXJzOltdLHRsc0luaXRGdW5jdGlvbnM6W10scHRocmVhZHM6e30sbmV4dFdvcmtlcklEOjEsaW5pdCgpe2lmKCFFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtQVGhyZWFkLmluaXRNYWluVGhyZWFkKCl9fSxpbml0TWFpblRocmVhZCgpe3ZhciBwdGhyZWFkUG9vbFNpemU9bmF2aWdhdG9yLmhhcmR3YXJlQ29uY3VycmVuY3k7d2hpbGUocHRocmVhZFBvb2xTaXplLS0pe1BUaHJlYWQuYWxsb2NhdGVVbnVzZWRXb3JrZXIoKX1hZGRPblByZVJ1bihhc3luYygpPT57dmFyIHB0aHJlYWRQb29sUmVhZHk9UFRocmVhZC5sb2FkV2FzbU1vZHVsZVRvQWxsV29ya2VycygpO2FkZFJ1bkRlcGVuZGVuY3koImxvYWRpbmctd29ya2VycyIpO2F3YWl0IHB0aHJlYWRQb29sUmVhZHk7cmVtb3ZlUnVuRGVwZW5kZW5jeSgibG9hZGluZy13b3JrZXJzIil9KX0sdGVybWluYXRlQWxsVGhyZWFkczooKT0+e2Fzc2VydCghRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCwiSW50ZXJuYWwgRXJyb3IhIHRlcm1pbmF0ZUFsbFRocmVhZHMoKSBjYW4gb25seSBldmVyIGJlIGNhbGxlZCBmcm9tIG1haW4gYXBwbGljYXRpb24gdGhyZWFkISIpO2Zvcih2YXIgd29ya2VyIG9mIFBUaHJlYWQucnVubmluZ1dvcmtlcnMpe3Rlcm1pbmF0ZVdvcmtlcih3b3JrZXIpfWZvcih2YXIgd29ya2VyIG9mIFBUaHJlYWQudW51c2VkV29ya2Vycyl7dGVybWluYXRlV29ya2VyKHdvcmtlcil9UFRocmVhZC51bnVzZWRXb3JrZXJzPVtdO1BUaHJlYWQucnVubmluZ1dvcmtlcnM9W107UFRocmVhZC5wdGhyZWFkcz17fX0scmV0dXJuV29ya2VyVG9Qb29sOndvcmtlcj0+e3ZhciBwdGhyZWFkX3B0cj13b3JrZXIucHRocmVhZF9wdHI7ZGVsZXRlIFBUaHJlYWQucHRocmVhZHNbcHRocmVhZF9wdHJdO1BUaHJlYWQudW51c2VkV29ya2Vycy5wdXNoKHdvcmtlcik7UFRocmVhZC5ydW5uaW5nV29ya2Vycy5zcGxpY2UoUFRocmVhZC5ydW5uaW5nV29ya2Vycy5pbmRleE9mKHdvcmtlciksMSk7d29ya2VyLnB0aHJlYWRfcHRyPTA7X19lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGEocHRocmVhZF9wdHIpfSx0aHJlYWRJbml0VExTKCl7UFRocmVhZC50bHNJbml0RnVuY3Rpb25zLmZvckVhY2goZj0+ZigpKX0sbG9hZFdhc21Nb2R1bGVUb1dvcmtlcjp3b3JrZXI9Pm5ldyBQcm9taXNlKG9uRmluaXNoZWRMb2FkaW5nPT57d29ya2VyLm9ubWVzc2FnZT1lPT57dmFyIGQ9ZVsiZGF0YSJdO3ZhciBjbWQ9ZC5jbWQ7aWYoZC50YXJnZXRUaHJlYWQmJmQudGFyZ2V0VGhyZWFkIT1fcHRocmVhZF9zZWxmKCkpe3ZhciB0YXJnZXRXb3JrZXI9UFRocmVhZC5wdGhyZWFkc1tkLnRhcmdldFRocmVhZF07aWYodGFyZ2V0V29ya2VyKXt0YXJnZXRXb3JrZXIucG9zdE1lc3NhZ2UoZCxkLnRyYW5zZmVyTGlzdCl9ZWxzZXtlcnIoYEludGVybmFsIGVycm9yISBXb3JrZXIgc2VudCBhIG1lc3NhZ2UgIiR7Y21kfSIgdG8gdGFyZ2V0IHB0aHJlYWQgJHtkLnRhcmdldFRocmVhZH0sIGJ1dCB0aGF0IHRocmVhZCBubyBsb25nZXIgZXhpc3RzIWApfXJldHVybn1pZihjbWQ9PT0iY2hlY2tNYWlsYm94Iil7Y2hlY2tNYWlsYm94KCl9ZWxzZSBpZihjbWQ9PT0ic3Bhd25UaHJlYWQiKXtzcGF3blRocmVhZChkKX1lbHNlIGlmKGNtZD09PSJjbGVhbnVwVGhyZWFkIil7Y2FsbFVzZXJDYWxsYmFjaygoKT0+Y2xlYW51cFRocmVhZChkLnRocmVhZCkpfWVsc2UgaWYoY21kPT09ImxvYWRlZCIpe3dvcmtlci5sb2FkZWQ9dHJ1ZTtvbkZpbmlzaGVkTG9hZGluZyh3b3JrZXIpfWVsc2UgaWYoZC50YXJnZXQ9PT0ic2V0aW1tZWRpYXRlIil7d29ya2VyLnBvc3RNZXNzYWdlKGQpfWVsc2UgaWYoY21kPT09ImNhbGxIYW5kbGVyIil7TW9kdWxlW2QuaGFuZGxlcl0oLi4uZC5hcmdzKX1lbHNlIGlmKGNtZCl7ZXJyKGB3b3JrZXIgc2VudCBhbiB1bmtub3duIGNvbW1hbmQgJHtjbWR9YCl9fTt3b3JrZXIub25lcnJvcj1lPT57dmFyIG1lc3NhZ2U9IndvcmtlciBzZW50IGFuIGVycm9yISI7aWYod29ya2VyLnB0aHJlYWRfcHRyKXttZXNzYWdlPWBQdGhyZWFkICR7cHRyVG9TdHJpbmcod29ya2VyLnB0aHJlYWRfcHRyKX0gc2VudCBhbiBlcnJvciFgfWVycihgJHttZXNzYWdlfSAke2UuZmlsZW5hbWV9OiR7ZS5saW5lbm99OiAke2UubWVzc2FnZX1gKTt0aHJvdyBlfTthc3NlcnQod2FzbU1lbW9yeSBpbnN0YW5jZW9mIFdlYkFzc2VtYmx5Lk1lbW9yeSwiV2ViQXNzZW1ibHkgbWVtb3J5IHNob3VsZCBoYXZlIGJlZW4gbG9hZGVkIGJ5IG5vdyEiKTthc3NlcnQod2FzbU1vZHVsZSBpbnN0YW5jZW9mIFdlYkFzc2VtYmx5Lk1vZHVsZSwiV2ViQXNzZW1ibHkgTW9kdWxlIHNob3VsZCBoYXZlIGJlZW4gbG9hZGVkIGJ5IG5vdyEiKTt2YXIgaGFuZGxlcnM9W107dmFyIGtub3duSGFuZGxlcnM9WyJvbkV4aXQiLCJvbkFib3J0IiwicHJpbnQiLCJwcmludEVyciJdO2Zvcih2YXIgaGFuZGxlciBvZiBrbm93bkhhbmRsZXJzKXtpZihNb2R1bGUucHJvcGVydHlJc0VudW1lcmFibGUoaGFuZGxlcikpe2hhbmRsZXJzLnB1c2goaGFuZGxlcil9fXdvcmtlci5wb3N0TWVzc2FnZSh7Y21kOiJsb2FkIixoYW5kbGVycyx3YXNtTWVtb3J5LHdhc21Nb2R1bGUsd29ya2VySUQ6d29ya2VyLndvcmtlcklEfSl9KSxhc3luYyBsb2FkV2FzbU1vZHVsZVRvQWxsV29ya2Vycygpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3JldHVybn1sZXQgcHRocmVhZFBvb2xSZWFkeT1Qcm9taXNlLmFsbChQVGhyZWFkLnVudXNlZFdvcmtlcnMubWFwKFBUaHJlYWQubG9hZFdhc21Nb2R1bGVUb1dvcmtlcikpO3JldHVybiBwdGhyZWFkUG9vbFJlYWR5fSxhbGxvY2F0ZVVudXNlZFdvcmtlcigpe3ZhciB3b3JrZXI7aWYoTW9kdWxlWyJtYWluU2NyaXB0VXJsT3JCbG9iIl0pe3ZhciBwdGhyZWFkTWFpbkpzPU1vZHVsZVsibWFpblNjcmlwdFVybE9yQmxvYiJdO2lmKHR5cGVvZiBwdGhyZWFkTWFpbkpzIT0ic3RyaW5nIil7cHRocmVhZE1haW5Kcz1VUkwuY3JlYXRlT2JqZWN0VVJMKHB0aHJlYWRNYWluSnMpfXdvcmtlcj1uZXcgV29ya2VyKHB0aHJlYWRNYWluSnMse3R5cGU6Im1vZHVsZSIsbmFtZToiZW0tcHRocmVhZC0iK1BUaHJlYWQubmV4dFdvcmtlcklEfSl9ZWxzZSB3b3JrZXI9bmV3IFdvcmtlcihuZXcgVVJMKCJjcF9zYXRfcnVudGltZV9hc3luY2lmeS5qcyIsaW1wb3J0Lm1ldGEudXJsKSx7dHlwZToibW9kdWxlIixuYW1lOiJlbS1wdGhyZWFkLSIrUFRocmVhZC5uZXh0V29ya2VySUR9KTt3b3JrZXIud29ya2VySUQ9UFRocmVhZC5uZXh0V29ya2VySUQrKztQVGhyZWFkLnVudXNlZFdvcmtlcnMucHVzaCh3b3JrZXIpfSxnZXROZXdXb3JrZXIoKXtpZihQVGhyZWFkLnVudXNlZFdvcmtlcnMubGVuZ3RoPT0wKXtlcnIoIlRyaWVkIHRvIHNwYXduIGEgbmV3IHRocmVhZCwgYnV0IHRoZSB0aHJlYWQgcG9vbCBpcyBleGhhdXN0ZWQuXG4iKyJUaGlzIG1pZ2h0IHJlc3VsdCBpbiBhIGRlYWRsb2NrIHVubGVzcyBzb21lIHRocmVhZHMgZXZlbnR1YWxseSBleGl0IG9yIHRoZSBjb2RlIGV4cGxpY2l0bHkgYnJlYWtzIG91dCB0byB0aGUgZXZlbnQgbG9vcC5cbiIrIklmIHlvdSB3YW50IHRvIGluY3JlYXNlIHRoZSBwb29sIHNpemUsIHVzZSBzZXR0aW5nIGAtc1BUSFJFQURfUE9PTF9TSVpFPS4uLmAuIik7cmV0dXJufXJldHVybiBQVGhyZWFkLnVudXNlZFdvcmtlcnMucG9wKCl9fTt2YXIgb25Qb3N0UnVucz1bXTt2YXIgYWRkT25Qb3N0UnVuPWNiPT5vblBvc3RSdW5zLnB1c2goY2IpO3ZhciBkeW5DYWxscz17fTt2YXIgZHluQ2FsbExlZ2FjeT0oc2lnLHB0cixhcmdzKT0+e3NpZz1zaWcucmVwbGFjZSgvcC9nLCJpIik7YXNzZXJ0KHNpZyBpbiBkeW5DYWxscyxgYmFkIGZ1bmN0aW9uIHBvaW50ZXIgdHlwZSAtIHNpZyBpcyBub3QgaW4gZHluQ2FsbHM6ICcke3NpZ30nYCk7aWYoYXJncz8ubGVuZ3RoKXthc3NlcnQoYXJncy5sZW5ndGg9PT1zaWcubGVuZ3RoLTEpfWVsc2V7YXNzZXJ0KHNpZy5sZW5ndGg9PTEpfXZhciBmPWR5bkNhbGxzW3NpZ107cmV0dXJuIGYocHRyLC4uLmFyZ3MpfTt2YXIgZHluQ2FsbD0oc2lnLHB0cixhcmdzPVtdLHByb21pc2luZz1mYWxzZSk9Pnthc3NlcnQocHRyLGBudWxsIGZ1bmN0aW9uIHBvaW50ZXIgaW4gZHluQ2FsbGApO2Fzc2VydCghcHJvbWlzaW5nLCJhc3luYyBkeW5DYWxsIGlzIG5vdCBzdXBwb3J0ZWQgaW4gdGhpcyBtb2RlIik7dmFyIHJ0bj1keW5DYWxsTGVnYWN5KHNpZyxwdHIsYXJncyk7ZnVuY3Rpb24gY29udmVydChydG4pe3JldHVybiBydG59cmV0dXJuIGNvbnZlcnQocnRuKX07ZnVuY3Rpb24gZXN0YWJsaXNoU3RhY2tTcGFjZShwdGhyZWFkX3B0cil7dmFyIHN0YWNrSGlnaD0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcHRocmVhZF9wdHIrNTI+PjJdO3ZhciBzdGFja1NpemU9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3B0aHJlYWRfcHRyKzU2Pj4yXTt2YXIgc3RhY2tMb3c9c3RhY2tIaWdoLXN0YWNrU2l6ZTthc3NlcnQoc3RhY2tIaWdoIT0wKTthc3NlcnQoc3RhY2tMb3chPTApO2Fzc2VydChzdGFja0hpZ2g+c3RhY2tMb3csInN0YWNrSGlnaCBtdXN0IGJlIGhpZ2hlciB0aGVuIHN0YWNrTG93Iik7X2Vtc2NyaXB0ZW5fc3RhY2tfc2V0X2xpbWl0cyhzdGFja0hpZ2gsc3RhY2tMb3cpO3N0YWNrUmVzdG9yZShzdGFja0hpZ2gpO3dyaXRlU3RhY2tDb29raWUoKX12YXIgaW52b2tlRW50cnlQb2ludD0ocHRyLGFyZyk9PntydW50aW1lS2VlcGFsaXZlQ291bnRlcj0wO25vRXhpdFJ1bnRpbWU9MDt2YXIgcmVzdWx0PShhMT0+ZHluQ2FsbF9paShwdHIsYTEpKShhcmcpO2NoZWNrU3RhY2tDb29raWUoKTtmdW5jdGlvbiBmaW5pc2gocmVzdWx0KXtpZihrZWVwUnVudGltZUFsaXZlKCkpe0VYSVRTVEFUVVM9cmVzdWx0O3JldHVybn1fX2Vtc2NyaXB0ZW5fdGhyZWFkX2V4aXQocmVzdWx0KX1maW5pc2gocmVzdWx0KX07aW52b2tlRW50cnlQb2ludC5pc0FzeW5jPXRydWU7dmFyIG5vRXhpdFJ1bnRpbWU9dHJ1ZTt2YXIgcmVnaXN0ZXJUTFNJbml0PXRsc0luaXRGdW5jPT5QVGhyZWFkLnRsc0luaXRGdW5jdGlvbnMucHVzaCh0bHNJbml0RnVuYyk7dmFyIHdhcm5PbmNlPXRleHQ9Pnt3YXJuT25jZS5zaG93bnx8PXt9O2lmKCF3YXJuT25jZS5zaG93blt0ZXh0XSl7d2Fybk9uY2Uuc2hvd25bdGV4dF09MTtlcnIodGV4dCl9fTt2YXIgd2FzbU1lbW9yeTt2YXIgVVRGOERlY29kZXI9Z2xvYmFsVGhpcy5UZXh0RGVjb2RlciYmbmV3IFRleHREZWNvZGVyO3ZhciBmaW5kU3RyaW5nRW5kPShoZWFwT3JBcnJheSxpZHgsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKT0+e3ZhciBtYXhJZHg9aWR4K21heEJ5dGVzVG9SZWFkO2lmKGlnbm9yZU51bClyZXR1cm4gbWF4SWR4O3doaWxlKGhlYXBPckFycmF5W2lkeF0mJiEoaWR4Pj1tYXhJZHgpKSsraWR4O3JldHVybiBpZHh9O3ZhciBVVEY4QXJyYXlUb1N0cmluZz0oaGVhcE9yQXJyYXksaWR4PTAsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKT0+e3ZhciBlbmRQdHI9ZmluZFN0cmluZ0VuZChoZWFwT3JBcnJheSxpZHgsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKTtpZihlbmRQdHItaWR4PjE2JiZoZWFwT3JBcnJheS5idWZmZXImJlVURjhEZWNvZGVyKXtyZXR1cm4gVVRGOERlY29kZXIuZGVjb2RlKGhlYXBPckFycmF5LmJ1ZmZlciBpbnN0YW5jZW9mIEFycmF5QnVmZmVyP2hlYXBPckFycmF5LnN1YmFycmF5KGlkeCxlbmRQdHIpOmhlYXBPckFycmF5LnNsaWNlKGlkeCxlbmRQdHIpKX12YXIgc3RyPSIiO3doaWxlKGlkeDxlbmRQdHIpe3ZhciB1MD1oZWFwT3JBcnJheVtpZHgrK107aWYoISh1MCYxMjgpKXtzdHIrPVN0cmluZy5mcm9tQ2hhckNvZGUodTApO2NvbnRpbnVlfXZhciB1MT1oZWFwT3JBcnJheVtpZHgrK10mNjM7aWYoKHUwJjIyNCk9PTE5Mil7c3RyKz1TdHJpbmcuZnJvbUNoYXJDb2RlKCh1MCYzMSk8PDZ8dTEpO2NvbnRpbnVlfXZhciB1Mj1oZWFwT3JBcnJheVtpZHgrK10mNjM7aWYoKHUwJjI0MCk9PTIyNCl7dTA9KHUwJjE1KTw8MTJ8dTE8PDZ8dTJ9ZWxzZXtpZigodTAmMjQ4KSE9MjQwKXdhcm5PbmNlKCJJbnZhbGlkIFVURi04IGxlYWRpbmcgYnl0ZSAiK3B0clRvU3RyaW5nKHUwKSsiIGVuY291bnRlcmVkIHdoZW4gZGVzZXJpYWxpemluZyBhIFVURi04IHN0cmluZyBpbiB3YXNtIG1lbW9yeSB0byBhIEpTIHN0cmluZyEiKTt1MD0odTAmNyk8PDE4fHUxPDwxMnx1Mjw8NnxoZWFwT3JBcnJheVtpZHgrK10mNjN9aWYodTA8NjU1MzYpe3N0cis9U3RyaW5nLmZyb21DaGFyQ29kZSh1MCl9ZWxzZXt2YXIgY2g9dTAtNjU1MzY7c3RyKz1TdHJpbmcuZnJvbUNoYXJDb2RlKDU1Mjk2fGNoPj4xMCw1NjMyMHxjaCYxMDIzKX19cmV0dXJuIHN0cn07dmFyIFVURjhUb1N0cmluZz0ocHRyLG1heEJ5dGVzVG9SZWFkLGlnbm9yZU51bCk9Pnthc3NlcnQodHlwZW9mIHB0cj09Im51bWJlciIsYFVURjhUb1N0cmluZyBleHBlY3RzIGEgbnVtYmVyIChnb3QgJHt0eXBlb2YgcHRyfSlgKTtyZXR1cm4gcHRyP1VURjhBcnJheVRvU3RyaW5nKChncm93TWVtVmlld3MoKSxIRUFQVTgpLHB0cixtYXhCeXRlc1RvUmVhZCxpZ25vcmVOdWwpOiIifTt2YXIgX19fYXNzZXJ0X2ZhaWw9KGNvbmRpdGlvbixmaWxlbmFtZSxsaW5lLGZ1bmMpPT5hYm9ydChgQXNzZXJ0aW9uIGZhaWxlZDogJHtVVEY4VG9TdHJpbmcoY29uZGl0aW9uKX0sIGF0OiBgK1tmaWxlbmFtZT9VVEY4VG9TdHJpbmcoZmlsZW5hbWUpOiJ1bmtub3duIGZpbGVuYW1lIixsaW5lLGZ1bmM/VVRGOFRvU3RyaW5nKGZ1bmMpOiJ1bmtub3duIGZ1bmN0aW9uIl0pO2NsYXNzIEV4Y2VwdGlvbkluZm97Y29uc3RydWN0b3IoZXhjUHRyKXt0aGlzLmV4Y1B0cj1leGNQdHI7dGhpcy5wdHI9ZXhjUHRyLTI0fXNldF90eXBlKHR5cGUpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cis0Pj4yXT10eXBlfWdldF90eXBlKCl7cmV0dXJuKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3RoaXMucHRyKzQ+PjJdfXNldF9kZXN0cnVjdG9yKGRlc3RydWN0b3Ipeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cis4Pj4yXT1kZXN0cnVjdG9yfWdldF9kZXN0cnVjdG9yKCl7cmV0dXJuKGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3RoaXMucHRyKzg+PjJdfXNldF9jYXVnaHQoY2F1Z2h0KXtjYXVnaHQ9Y2F1Z2h0PzE6MDsoZ3Jvd01lbVZpZXdzKCksSEVBUDgpW3RoaXMucHRyKzEyXT1jYXVnaHR9Z2V0X2NhdWdodCgpe3JldHVybihncm93TWVtVmlld3MoKSxIRUFQOClbdGhpcy5wdHIrMTJdIT0wfXNldF9yZXRocm93bihyZXRocm93bil7cmV0aHJvd249cmV0aHJvd24/MTowOyhncm93TWVtVmlld3MoKSxIRUFQOClbdGhpcy5wdHIrMTNdPXJldGhyb3dufWdldF9yZXRocm93bigpe3JldHVybihncm93TWVtVmlld3MoKSxIRUFQOClbdGhpcy5wdHIrMTNdIT0wfWluaXQodHlwZSxkZXN0cnVjdG9yKXt0aGlzLnNldF9hZGp1c3RlZF9wdHIoMCk7dGhpcy5zZXRfdHlwZSh0eXBlKTt0aGlzLnNldF9kZXN0cnVjdG9yKGRlc3RydWN0b3IpfXNldF9hZGp1c3RlZF9wdHIoYWRqdXN0ZWRQdHIpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cisxNj4+Ml09YWRqdXN0ZWRQdHJ9Z2V0X2FkanVzdGVkX3B0cigpe3JldHVybihncm93TWVtVmlld3MoKSxIRUFQVTMyKVt0aGlzLnB0cisxNj4+Ml19fXZhciBleGNlcHRpb25MYXN0PTA7dmFyIHVuY2F1Z2h0RXhjZXB0aW9uQ291bnQ9MDt2YXIgX19fY3hhX3Rocm93PShwdHIsdHlwZSxkZXN0cnVjdG9yKT0+e3ZhciBpbmZvPW5ldyBFeGNlcHRpb25JbmZvKHB0cik7aW5mby5pbml0KHR5cGUsZGVzdHJ1Y3Rvcik7ZXhjZXB0aW9uTGFzdD1wdHI7dW5jYXVnaHRFeGNlcHRpb25Db3VudCsrO2Fzc2VydChmYWxzZSwiRXhjZXB0aW9uIHRocm93biwgYnV0IGV4Y2VwdGlvbiBjYXRjaGluZyBpcyBub3QgZW5hYmxlZC4gQ29tcGlsZSB3aXRoIC1zTk9fRElTQUJMRV9FWENFUFRJT05fQ0FUQ0hJTkcgb3IgLXNFWENFUFRJT05fQ0FUQ0hJTkdfQUxMT1dFRD1bLi5dIHRvIGNhdGNoLiIpfTtmdW5jdGlvbiBwdGhyZWFkQ3JlYXRlUHJveGllZChwdGhyZWFkX3B0cixhdHRyLHN0YXJ0Um91dGluZSxhcmcpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDIsMCwxLHB0aHJlYWRfcHRyLGF0dHIsc3RhcnRSb3V0aW5lLGFyZyk7cmV0dXJuIF9fX3B0aHJlYWRfY3JlYXRlX2pzKHB0aHJlYWRfcHRyLGF0dHIsc3RhcnRSb3V0aW5lLGFyZyl9dmFyIF9lbXNjcmlwdGVuX2hhc190aHJlYWRpbmdfc3VwcG9ydD0oKT0+ISFnbG9iYWxUaGlzLlNoYXJlZEFycmF5QnVmZmVyO3ZhciBfX19wdGhyZWFkX2NyZWF0ZV9qcz0ocHRocmVhZF9wdHIsYXR0cixzdGFydFJvdXRpbmUsYXJnKT0+e2lmKCFfZW1zY3JpcHRlbl9oYXNfdGhyZWFkaW5nX3N1cHBvcnQoKSl7ZGJnKCJwdGhyZWFkX2NyZWF0ZTogZW52aXJvbm1lbnQgZG9lcyBub3Qgc3VwcG9ydCBTaGFyZWRBcnJheUJ1ZmZlciwgcHRocmVhZHMgYXJlIG5vdCBhdmFpbGFibGUiKTtyZXR1cm4gNn12YXIgdHJhbnNmZXJMaXN0PVtdO3ZhciBlcnJvcj0wO2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQmJih0cmFuc2Zlckxpc3QubGVuZ3RoPT09MHx8ZXJyb3IpKXtyZXR1cm4gcHRocmVhZENyZWF0ZVByb3hpZWQocHRocmVhZF9wdHIsYXR0cixzdGFydFJvdXRpbmUsYXJnKX1pZihlcnJvcilyZXR1cm4gZXJyb3I7dmFyIHRocmVhZFBhcmFtcz17c3RhcnRSb3V0aW5lLHB0aHJlYWRfcHRyLGFyZyx0cmFuc2Zlckxpc3R9O2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpe3RocmVhZFBhcmFtcy5jbWQ9InNwYXduVGhyZWFkIjtwb3N0TWVzc2FnZSh0aHJlYWRQYXJhbXMsdHJhbnNmZXJMaXN0KTtyZXR1cm4gMH1yZXR1cm4gc3Bhd25UaHJlYWQodGhyZWFkUGFyYW1zKX07dmFyIHN5c2NhbGxHZXRWYXJhcmdJPSgpPT57YXNzZXJ0KFNZU0NBTExTLnZhcmFyZ3MhPXVuZGVmaW5lZCk7dmFyIHJldD0oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVsrU1lTQ0FMTFMudmFyYXJncz4+Ml07U1lTQ0FMTFMudmFyYXJncys9NDtyZXR1cm4gcmV0fTt2YXIgc3lzY2FsbEdldFZhcmFyZ1A9c3lzY2FsbEdldFZhcmFyZ0k7dmFyIFBBVEg9e2lzQWJzOnBhdGg9PnBhdGguY2hhckF0KDApPT09Ii8iLHNwbGl0UGF0aDpmaWxlbmFtZT0+e3ZhciBzcGxpdFBhdGhSZT0vXihcLz98KShbXHNcU10qPykoKD86XC57MSwyfXxbXlwvXSs/fCkoXC5bXi5cL10qfCkpKD86W1wvXSopJC87cmV0dXJuIHNwbGl0UGF0aFJlLmV4ZWMoZmlsZW5hbWUpLnNsaWNlKDEpfSxub3JtYWxpemVBcnJheToocGFydHMsYWxsb3dBYm92ZVJvb3QpPT57dmFyIHVwPTA7Zm9yKHZhciBpPXBhcnRzLmxlbmd0aC0xO2k+PTA7aS0tKXt2YXIgbGFzdD1wYXJ0c1tpXTtpZihsYXN0PT09Ii4iKXtwYXJ0cy5zcGxpY2UoaSwxKX1lbHNlIGlmKGxhc3Q9PT0iLi4iKXtwYXJ0cy5zcGxpY2UoaSwxKTt1cCsrfWVsc2UgaWYodXApe3BhcnRzLnNwbGljZShpLDEpO3VwLS19fWlmKGFsbG93QWJvdmVSb290KXtmb3IoO3VwO3VwLS0pe3BhcnRzLnVuc2hpZnQoIi4uIil9fXJldHVybiBwYXJ0c30sbm9ybWFsaXplOnBhdGg9Pnt2YXIgaXNBYnNvbHV0ZT1QQVRILmlzQWJzKHBhdGgpLHRyYWlsaW5nU2xhc2g9cGF0aC5zbGljZSgtMSk9PT0iLyI7cGF0aD1QQVRILm5vcm1hbGl6ZUFycmF5KHBhdGguc3BsaXQoIi8iKS5maWx0ZXIocD0+ISFwKSwhaXNBYnNvbHV0ZSkuam9pbigiLyIpO2lmKCFwYXRoJiYhaXNBYnNvbHV0ZSl7cGF0aD0iLiJ9aWYocGF0aCYmdHJhaWxpbmdTbGFzaCl7cGF0aCs9Ii8ifXJldHVybihpc0Fic29sdXRlPyIvIjoiIikrcGF0aH0sZGlybmFtZTpwYXRoPT57dmFyIHJlc3VsdD1QQVRILnNwbGl0UGF0aChwYXRoKSxyb290PXJlc3VsdFswXSxkaXI9cmVzdWx0WzFdO2lmKCFyb290JiYhZGlyKXtyZXR1cm4iLiJ9aWYoZGlyKXtkaXI9ZGlyLnNsaWNlKDAsLTEpfXJldHVybiByb290K2Rpcn0sYmFzZW5hbWU6cGF0aD0+cGF0aCYmcGF0aC5tYXRjaCgvKFteXC9dK3xcLylcLyokLylbMV0sam9pbjooLi4ucGF0aHMpPT5QQVRILm5vcm1hbGl6ZShwYXRocy5qb2luKCIvIikpLGpvaW4yOihsLHIpPT5QQVRILm5vcm1hbGl6ZShsKyIvIityKX07dmFyIGluaXRSYW5kb21GaWxsPSgpPT52aWV3PT52aWV3LnNldChjcnlwdG8uZ2V0UmFuZG9tVmFsdWVzKG5ldyBVaW50OEFycmF5KHZpZXcuYnl0ZUxlbmd0aCkpKTt2YXIgcmFuZG9tRmlsbD12aWV3PT57KHJhbmRvbUZpbGw9aW5pdFJhbmRvbUZpbGwoKSkodmlldyl9O3ZhciBQQVRIX0ZTPXtyZXNvbHZlOiguLi5hcmdzKT0+e3ZhciByZXNvbHZlZFBhdGg9IiIscmVzb2x2ZWRBYnNvbHV0ZT1mYWxzZTtmb3IodmFyIGk9YXJncy5sZW5ndGgtMTtpPj0tMSYmIXJlc29sdmVkQWJzb2x1dGU7aS0tKXt2YXIgcGF0aD1pPj0wP2FyZ3NbaV06RlMuY3dkKCk7aWYodHlwZW9mIHBhdGghPSJzdHJpbmciKXt0aHJvdyBuZXcgVHlwZUVycm9yKCJBcmd1bWVudHMgdG8gcGF0aC5yZXNvbHZlIG11c3QgYmUgc3RyaW5ncyIpfWVsc2UgaWYoIXBhdGgpe3JldHVybiIifXJlc29sdmVkUGF0aD1wYXRoKyIvIityZXNvbHZlZFBhdGg7cmVzb2x2ZWRBYnNvbHV0ZT1QQVRILmlzQWJzKHBhdGgpfXJlc29sdmVkUGF0aD1QQVRILm5vcm1hbGl6ZUFycmF5KHJlc29sdmVkUGF0aC5zcGxpdCgiLyIpLmZpbHRlcihwPT4hIXApLCFyZXNvbHZlZEFic29sdXRlKS5qb2luKCIvIik7cmV0dXJuKHJlc29sdmVkQWJzb2x1dGU/Ii8iOiIiKStyZXNvbHZlZFBhdGh8fCIuIn0scmVsYXRpdmU6KGZyb20sdG8pPT57ZnJvbT1QQVRIX0ZTLnJlc29sdmUoZnJvbSkuc2xpY2UoMSk7dG89UEFUSF9GUy5yZXNvbHZlKHRvKS5zbGljZSgxKTtmdW5jdGlvbiB0cmltKGFycil7dmFyIHN0YXJ0PTA7Zm9yKDtzdGFydDxhcnIubGVuZ3RoO3N0YXJ0Kyspe2lmKGFycltzdGFydF0hPT0iIilicmVha312YXIgZW5kPWFyci5sZW5ndGgtMTtmb3IoO2VuZD49MDtlbmQtLSl7aWYoYXJyW2VuZF0hPT0iIilicmVha31pZihzdGFydD5lbmQpcmV0dXJuW107cmV0dXJuIGFyci5zbGljZShzdGFydCxlbmQtc3RhcnQrMSl9dmFyIGZyb21QYXJ0cz10cmltKGZyb20uc3BsaXQoIi8iKSk7dmFyIHRvUGFydHM9dHJpbSh0by5zcGxpdCgiLyIpKTt2YXIgbGVuZ3RoPU1hdGgubWluKGZyb21QYXJ0cy5sZW5ndGgsdG9QYXJ0cy5sZW5ndGgpO3ZhciBzYW1lUGFydHNMZW5ndGg9bGVuZ3RoO2Zvcih2YXIgaT0wO2k8bGVuZ3RoO2krKyl7aWYoZnJvbVBhcnRzW2ldIT09dG9QYXJ0c1tpXSl7c2FtZVBhcnRzTGVuZ3RoPWk7YnJlYWt9fXZhciBvdXRwdXRQYXJ0cz1bXTtmb3IodmFyIGk9c2FtZVBhcnRzTGVuZ3RoO2k8ZnJvbVBhcnRzLmxlbmd0aDtpKyspe291dHB1dFBhcnRzLnB1c2goIi4uIil9b3V0cHV0UGFydHM9b3V0cHV0UGFydHMuY29uY2F0KHRvUGFydHMuc2xpY2Uoc2FtZVBhcnRzTGVuZ3RoKSk7cmV0dXJuIG91dHB1dFBhcnRzLmpvaW4oIi8iKX19O3ZhciBGU19zdGRpbl9nZXRDaGFyX2J1ZmZlcj1bXTt2YXIgbGVuZ3RoQnl0ZXNVVEY4PXN0cj0+e3ZhciBsZW49MDtmb3IodmFyIGk9MDtpPHN0ci5sZW5ndGg7KytpKXt2YXIgYz1zdHIuY2hhckNvZGVBdChpKTtpZihjPD0xMjcpe2xlbisrfWVsc2UgaWYoYzw9MjA0Nyl7bGVuKz0yfWVsc2UgaWYoYz49NTUyOTYmJmM8PTU3MzQzKXtsZW4rPTQ7KytpfWVsc2V7bGVuKz0zfX1yZXR1cm4gbGVufTt2YXIgc3RyaW5nVG9VVEY4QXJyYXk9KHN0cixoZWFwLG91dElkeCxtYXhCeXRlc1RvV3JpdGUpPT57YXNzZXJ0KHR5cGVvZiBzdHI9PT0ic3RyaW5nIixgc3RyaW5nVG9VVEY4QXJyYXkgZXhwZWN0cyBhIHN0cmluZyAoZ290ICR7dHlwZW9mIHN0cn0pYCk7aWYoIShtYXhCeXRlc1RvV3JpdGU+MCkpcmV0dXJuIDA7dmFyIHN0YXJ0SWR4PW91dElkeDt2YXIgZW5kSWR4PW91dElkeCttYXhCeXRlc1RvV3JpdGUtMTtmb3IodmFyIGk9MDtpPHN0ci5sZW5ndGg7KytpKXt2YXIgdT1zdHIuY29kZVBvaW50QXQoaSk7aWYodTw9MTI3KXtpZihvdXRJZHg+PWVuZElkeClicmVhaztoZWFwW291dElkeCsrXT11fWVsc2UgaWYodTw9MjA0Nyl7aWYob3V0SWR4KzE+PWVuZElkeClicmVhaztoZWFwW291dElkeCsrXT0xOTJ8dT4+NjtoZWFwW291dElkeCsrXT0xMjh8dSY2M31lbHNlIGlmKHU8PTY1NTM1KXtpZihvdXRJZHgrMj49ZW5kSWR4KWJyZWFrO2hlYXBbb3V0SWR4KytdPTIyNHx1Pj4xMjtoZWFwW291dElkeCsrXT0xMjh8dT4+NiY2MztoZWFwW291dElkeCsrXT0xMjh8dSY2M31lbHNle2lmKG91dElkeCszPj1lbmRJZHgpYnJlYWs7aWYodT4xMTE0MTExKXdhcm5PbmNlKCJJbnZhbGlkIFVuaWNvZGUgY29kZSBwb2ludCAiK3B0clRvU3RyaW5nKHUpKyIgZW5jb3VudGVyZWQgd2hlbiBzZXJpYWxpemluZyBhIEpTIHN0cmluZyB0byBhIFVURi04IHN0cmluZyBpbiB3YXNtIG1lbW9yeSEgKFZhbGlkIHVuaWNvZGUgY29kZSBwb2ludHMgc2hvdWxkIGJlIGluIHJhbmdlIDAtMHgxMEZGRkYpLiIpO2hlYXBbb3V0SWR4KytdPTI0MHx1Pj4xODtoZWFwW291dElkeCsrXT0xMjh8dT4+MTImNjM7aGVhcFtvdXRJZHgrK109MTI4fHU+PjYmNjM7aGVhcFtvdXRJZHgrK109MTI4fHUmNjM7aSsrfX1oZWFwW291dElkeF09MDtyZXR1cm4gb3V0SWR4LXN0YXJ0SWR4fTt2YXIgaW50QXJyYXlGcm9tU3RyaW5nPShzdHJpbmd5LGRvbnRBZGROdWxsLGxlbmd0aCk9Pnt2YXIgbGVuPWxlbmd0aD4wP2xlbmd0aDpsZW5ndGhCeXRlc1VURjgoc3RyaW5neSkrMTt2YXIgdThhcnJheT1uZXcgQXJyYXkobGVuKTt2YXIgbnVtQnl0ZXNXcml0dGVuPXN0cmluZ1RvVVRGOEFycmF5KHN0cmluZ3ksdThhcnJheSwwLHU4YXJyYXkubGVuZ3RoKTtpZihkb250QWRkTnVsbCl1OGFycmF5Lmxlbmd0aD1udW1CeXRlc1dyaXR0ZW47cmV0dXJuIHU4YXJyYXl9O3ZhciBGU19zdGRpbl9nZXRDaGFyPSgpPT57aWYoIUZTX3N0ZGluX2dldENoYXJfYnVmZmVyLmxlbmd0aCl7dmFyIHJlc3VsdD1udWxsO2lmKGdsb2JhbFRoaXMud2luZG93Py5wcm9tcHQpe3Jlc3VsdD13aW5kb3cucHJvbXB0KCJJbnB1dDogIik7aWYocmVzdWx0IT09bnVsbCl7cmVzdWx0Kz0iXG4ifX1lbHNle31pZighcmVzdWx0KXtyZXR1cm4gbnVsbH1GU19zdGRpbl9nZXRDaGFyX2J1ZmZlcj1pbnRBcnJheUZyb21TdHJpbmcocmVzdWx0LHRydWUpfXJldHVybiBGU19zdGRpbl9nZXRDaGFyX2J1ZmZlci5zaGlmdCgpfTt2YXIgVFRZPXt0dHlzOltdLGluaXQoKXt9LHNodXRkb3duKCl7fSxyZWdpc3RlcihkZXYsb3BzKXtUVFkudHR5c1tkZXZdPXtpbnB1dDpbXSxvdXRwdXQ6W10sb3BzfTtGUy5yZWdpc3RlckRldmljZShkZXYsVFRZLnN0cmVhbV9vcHMpfSxzdHJlYW1fb3BzOntvcGVuKHN0cmVhbSl7dmFyIHR0eT1UVFkudHR5c1tzdHJlYW0ubm9kZS5yZGV2XTtpZighdHR5KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0Myl9c3RyZWFtLnR0eT10dHk7c3RyZWFtLnNlZWthYmxlPWZhbHNlfSxjbG9zZShzdHJlYW0pe3N0cmVhbS50dHkub3BzLmZzeW5jKHN0cmVhbS50dHkpfSxmc3luYyhzdHJlYW0pe3N0cmVhbS50dHkub3BzLmZzeW5jKHN0cmVhbS50dHkpfSxyZWFkKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3Mpe2lmKCFzdHJlYW0udHR5fHwhc3RyZWFtLnR0eS5vcHMuZ2V0X2NoYXIpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYwKX12YXIgYnl0ZXNSZWFkPTA7Zm9yKHZhciBpPTA7aTxsZW5ndGg7aSsrKXt2YXIgcmVzdWx0O3RyeXtyZXN1bHQ9c3RyZWFtLnR0eS5vcHMuZ2V0X2NoYXIoc3RyZWFtLnR0eSl9Y2F0Y2goZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjkpfWlmKHJlc3VsdD09PXVuZGVmaW5lZCYmYnl0ZXNSZWFkPT09MCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNil9aWYocmVzdWx0PT09bnVsbHx8cmVzdWx0PT09dW5kZWZpbmVkKWJyZWFrO2J5dGVzUmVhZCsrO2J1ZmZlcltvZmZzZXQraV09cmVzdWx0fWlmKGJ5dGVzUmVhZCl7c3RyZWFtLm5vZGUuYXRpbWU9RGF0ZS5ub3coKX1yZXR1cm4gYnl0ZXNSZWFkfSx3cml0ZShzdHJlYW0sYnVmZmVyLG9mZnNldCxsZW5ndGgscG9zKXtpZighc3RyZWFtLnR0eXx8IXN0cmVhbS50dHkub3BzLnB1dF9jaGFyKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig2MCl9dHJ5e2Zvcih2YXIgaT0wO2k8bGVuZ3RoO2krKyl7c3RyZWFtLnR0eS5vcHMucHV0X2NoYXIoc3RyZWFtLnR0eSxidWZmZXJbb2Zmc2V0K2ldKX19Y2F0Y2goZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjkpfWlmKGxlbmd0aCl7c3RyZWFtLm5vZGUubXRpbWU9c3RyZWFtLm5vZGUuY3RpbWU9RGF0ZS5ub3coKX1yZXR1cm4gaX19LGRlZmF1bHRfdHR5X29wczp7Z2V0X2NoYXIodHR5KXtyZXR1cm4gRlNfc3RkaW5fZ2V0Q2hhcigpfSxwdXRfY2hhcih0dHksdmFsKXtpZih2YWw9PT1udWxsfHx2YWw9PT0xMCl7b3V0KFVURjhBcnJheVRvU3RyaW5nKHR0eS5vdXRwdXQpKTt0dHkub3V0cHV0PVtdfWVsc2V7aWYodmFsIT0wKXR0eS5vdXRwdXQucHVzaCh2YWwpfX0sZnN5bmModHR5KXtpZih0dHkub3V0cHV0Py5sZW5ndGg+MCl7b3V0KFVURjhBcnJheVRvU3RyaW5nKHR0eS5vdXRwdXQpKTt0dHkub3V0cHV0PVtdfX0saW9jdGxfdGNnZXRzKHR0eSl7cmV0dXJue2NfaWZsYWc6MjU4NTYsY19vZmxhZzo1LGNfY2ZsYWc6MTkxLGNfbGZsYWc6MzUzODcsY19jYzpbMywyOCwxMjcsMjEsNCwwLDEsMCwxNywxOSwyNiwwLDE4LDE1LDIzLDIyLDAsMCwwLDAsMCwwLDAsMCwwLDAsMCwwLDAsMCwwLDBdfX0saW9jdGxfdGNzZXRzKHR0eSxvcHRpb25hbF9hY3Rpb25zLGRhdGEpe3JldHVybiAwfSxpb2N0bF90aW9jZ3dpbnN6KHR0eSl7cmV0dXJuWzI0LDgwXX19LGRlZmF1bHRfdHR5MV9vcHM6e3B1dF9jaGFyKHR0eSx2YWwpe2lmKHZhbD09PW51bGx8fHZhbD09PTEwKXtlcnIoVVRGOEFycmF5VG9TdHJpbmcodHR5Lm91dHB1dCkpO3R0eS5vdXRwdXQ9W119ZWxzZXtpZih2YWwhPTApdHR5Lm91dHB1dC5wdXNoKHZhbCl9fSxmc3luYyh0dHkpe2lmKHR0eS5vdXRwdXQ/Lmxlbmd0aD4wKXtlcnIoVVRGOEFycmF5VG9TdHJpbmcodHR5Lm91dHB1dCkpO3R0eS5vdXRwdXQ9W119fX19O3ZhciB6ZXJvTWVtb3J5PShwdHIsc2l6ZSk9Pihncm93TWVtVmlld3MoKSxIRUFQVTgpLmZpbGwoMCxwdHIscHRyK3NpemUpO3ZhciBhbGlnbk1lbW9yeT0oc2l6ZSxhbGlnbm1lbnQpPT57YXNzZXJ0KGFsaWdubWVudCwiYWxpZ25tZW50IGFyZ3VtZW50IGlzIHJlcXVpcmVkIik7cmV0dXJuIE1hdGguY2VpbChzaXplL2FsaWdubWVudCkqYWxpZ25tZW50fTt2YXIgbW1hcEFsbG9jPXNpemU9PntzaXplPWFsaWduTWVtb3J5KHNpemUsNjU1MzYpO3ZhciBwdHI9X2Vtc2NyaXB0ZW5fYnVpbHRpbl9tZW1hbGlnbig2NTUzNixzaXplKTtpZihwdHIpemVyb01lbW9yeShwdHIsc2l6ZSk7cmV0dXJuIHB0cn07dmFyIE1FTUZTPXtvcHNfdGFibGU6bnVsbCxtb3VudChtb3VudCl7cmV0dXJuIE1FTUZTLmNyZWF0ZU5vZGUobnVsbCwiLyIsMTY4OTUsMCl9LGNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxkZXYpe2lmKEZTLmlzQmxrZGV2KG1vZGUpfHxGUy5pc0ZJRk8obW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYzKX1NRU1GUy5vcHNfdGFibGV8fD17ZGlyOntub2RlOntnZXRhdHRyOk1FTUZTLm5vZGVfb3BzLmdldGF0dHIsc2V0YXR0cjpNRU1GUy5ub2RlX29wcy5zZXRhdHRyLGxvb2t1cDpNRU1GUy5ub2RlX29wcy5sb29rdXAsbWtub2Q6TUVNRlMubm9kZV9vcHMubWtub2QscmVuYW1lOk1FTUZTLm5vZGVfb3BzLnJlbmFtZSx1bmxpbms6TUVNRlMubm9kZV9vcHMudW5saW5rLHJtZGlyOk1FTUZTLm5vZGVfb3BzLnJtZGlyLHJlYWRkaXI6TUVNRlMubm9kZV9vcHMucmVhZGRpcixzeW1saW5rOk1FTUZTLm5vZGVfb3BzLnN5bWxpbmt9LHN0cmVhbTp7bGxzZWVrOk1FTUZTLnN0cmVhbV9vcHMubGxzZWVrfX0sZmlsZTp7bm9kZTp7Z2V0YXR0cjpNRU1GUy5ub2RlX29wcy5nZXRhdHRyLHNldGF0dHI6TUVNRlMubm9kZV9vcHMuc2V0YXR0cn0sc3RyZWFtOntsbHNlZWs6TUVNRlMuc3RyZWFtX29wcy5sbHNlZWsscmVhZDpNRU1GUy5zdHJlYW1fb3BzLnJlYWQsd3JpdGU6TUVNRlMuc3RyZWFtX29wcy53cml0ZSxtbWFwOk1FTUZTLnN0cmVhbV9vcHMubW1hcCxtc3luYzpNRU1GUy5zdHJlYW1fb3BzLm1zeW5jfX0sbGluazp7bm9kZTp7Z2V0YXR0cjpNRU1GUy5ub2RlX29wcy5nZXRhdHRyLHNldGF0dHI6TUVNRlMubm9kZV9vcHMuc2V0YXR0cixyZWFkbGluazpNRU1GUy5ub2RlX29wcy5yZWFkbGlua30sc3RyZWFtOnt9fSxjaHJkZXY6e25vZGU6e2dldGF0dHI6TUVNRlMubm9kZV9vcHMuZ2V0YXR0cixzZXRhdHRyOk1FTUZTLm5vZGVfb3BzLnNldGF0dHJ9LHN0cmVhbTpGUy5jaHJkZXZfc3RyZWFtX29wc319O3ZhciBub2RlPUZTLmNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxkZXYpO2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe25vZGUubm9kZV9vcHM9TUVNRlMub3BzX3RhYmxlLmRpci5ub2RlO25vZGUuc3RyZWFtX29wcz1NRU1GUy5vcHNfdGFibGUuZGlyLnN0cmVhbTtub2RlLmNvbnRlbnRzPXt9fWVsc2UgaWYoRlMuaXNGaWxlKG5vZGUubW9kZSkpe25vZGUubm9kZV9vcHM9TUVNRlMub3BzX3RhYmxlLmZpbGUubm9kZTtub2RlLnN0cmVhbV9vcHM9TUVNRlMub3BzX3RhYmxlLmZpbGUuc3RyZWFtO25vZGUudXNlZEJ5dGVzPTA7bm9kZS5jb250ZW50cz1udWxsfWVsc2UgaWYoRlMuaXNMaW5rKG5vZGUubW9kZSkpe25vZGUubm9kZV9vcHM9TUVNRlMub3BzX3RhYmxlLmxpbmsubm9kZTtub2RlLnN0cmVhbV9vcHM9TUVNRlMub3BzX3RhYmxlLmxpbmsuc3RyZWFtfWVsc2UgaWYoRlMuaXNDaHJkZXYobm9kZS5tb2RlKSl7bm9kZS5ub2RlX29wcz1NRU1GUy5vcHNfdGFibGUuY2hyZGV2Lm5vZGU7bm9kZS5zdHJlYW1fb3BzPU1FTUZTLm9wc190YWJsZS5jaHJkZXYuc3RyZWFtfW5vZGUuYXRpbWU9bm9kZS5tdGltZT1ub2RlLmN0aW1lPURhdGUubm93KCk7aWYocGFyZW50KXtwYXJlbnQuY29udGVudHNbbmFtZV09bm9kZTtwYXJlbnQuYXRpbWU9cGFyZW50Lm10aW1lPXBhcmVudC5jdGltZT1ub2RlLmF0aW1lfXJldHVybiBub2RlfSxnZXRGaWxlRGF0YUFzVHlwZWRBcnJheShub2RlKXtpZighbm9kZS5jb250ZW50cylyZXR1cm4gbmV3IFVpbnQ4QXJyYXkoMCk7aWYobm9kZS5jb250ZW50cy5zdWJhcnJheSlyZXR1cm4gbm9kZS5jb250ZW50cy5zdWJhcnJheSgwLG5vZGUudXNlZEJ5dGVzKTtyZXR1cm4gbmV3IFVpbnQ4QXJyYXkobm9kZS5jb250ZW50cyl9LGV4cGFuZEZpbGVTdG9yYWdlKG5vZGUsbmV3Q2FwYWNpdHkpe3ZhciBwcmV2Q2FwYWNpdHk9bm9kZS5jb250ZW50cz9ub2RlLmNvbnRlbnRzLmxlbmd0aDowO2lmKHByZXZDYXBhY2l0eT49bmV3Q2FwYWNpdHkpcmV0dXJuO3ZhciBDQVBBQ0lUWV9ET1VCTElOR19NQVg9MTAyNCoxMDI0O25ld0NhcGFjaXR5PU1hdGgubWF4KG5ld0NhcGFjaXR5LHByZXZDYXBhY2l0eSoocHJldkNhcGFjaXR5PENBUEFDSVRZX0RPVUJMSU5HX01BWD8yOjEuMTI1KT4+PjApO2lmKHByZXZDYXBhY2l0eSE9MCluZXdDYXBhY2l0eT1NYXRoLm1heChuZXdDYXBhY2l0eSwyNTYpO3ZhciBvbGRDb250ZW50cz1ub2RlLmNvbnRlbnRzO25vZGUuY29udGVudHM9bmV3IFVpbnQ4QXJyYXkobmV3Q2FwYWNpdHkpO2lmKG5vZGUudXNlZEJ5dGVzPjApbm9kZS5jb250ZW50cy5zZXQob2xkQ29udGVudHMuc3ViYXJyYXkoMCxub2RlLnVzZWRCeXRlcyksMCl9LHJlc2l6ZUZpbGVTdG9yYWdlKG5vZGUsbmV3U2l6ZSl7aWYobm9kZS51c2VkQnl0ZXM9PW5ld1NpemUpcmV0dXJuO2lmKG5ld1NpemU9PTApe25vZGUuY29udGVudHM9bnVsbDtub2RlLnVzZWRCeXRlcz0wfWVsc2V7dmFyIG9sZENvbnRlbnRzPW5vZGUuY29udGVudHM7bm9kZS5jb250ZW50cz1uZXcgVWludDhBcnJheShuZXdTaXplKTtpZihvbGRDb250ZW50cyl7bm9kZS5jb250ZW50cy5zZXQob2xkQ29udGVudHMuc3ViYXJyYXkoMCxNYXRoLm1pbihuZXdTaXplLG5vZGUudXNlZEJ5dGVzKSkpfW5vZGUudXNlZEJ5dGVzPW5ld1NpemV9fSxub2RlX29wczp7Z2V0YXR0cihub2RlKXt2YXIgYXR0cj17fTthdHRyLmRldj1GUy5pc0NocmRldihub2RlLm1vZGUpP25vZGUuaWQ6MTthdHRyLmlubz1ub2RlLmlkO2F0dHIubW9kZT1ub2RlLm1vZGU7YXR0ci5ubGluaz0xO2F0dHIudWlkPTA7YXR0ci5naWQ9MDthdHRyLnJkZXY9bm9kZS5yZGV2O2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe2F0dHIuc2l6ZT00MDk2fWVsc2UgaWYoRlMuaXNGaWxlKG5vZGUubW9kZSkpe2F0dHIuc2l6ZT1ub2RlLnVzZWRCeXRlc31lbHNlIGlmKEZTLmlzTGluayhub2RlLm1vZGUpKXthdHRyLnNpemU9bm9kZS5saW5rLmxlbmd0aH1lbHNle2F0dHIuc2l6ZT0wfWF0dHIuYXRpbWU9bmV3IERhdGUobm9kZS5hdGltZSk7YXR0ci5tdGltZT1uZXcgRGF0ZShub2RlLm10aW1lKTthdHRyLmN0aW1lPW5ldyBEYXRlKG5vZGUuY3RpbWUpO2F0dHIuYmxrc2l6ZT00MDk2O2F0dHIuYmxvY2tzPU1hdGguY2VpbChhdHRyLnNpemUvYXR0ci5ibGtzaXplKTtyZXR1cm4gYXR0cn0sc2V0YXR0cihub2RlLGF0dHIpe2Zvcihjb25zdCBrZXkgb2ZbIm1vZGUiLCJhdGltZSIsIm10aW1lIiwiY3RpbWUiXSl7aWYoYXR0cltrZXldIT1udWxsKXtub2RlW2tleV09YXR0cltrZXldfX1pZihhdHRyLnNpemUhPT11bmRlZmluZWQpe01FTUZTLnJlc2l6ZUZpbGVTdG9yYWdlKG5vZGUsYXR0ci5zaXplKX19LGxvb2t1cChwYXJlbnQsbmFtZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDQpfSxta25vZChwYXJlbnQsbmFtZSxtb2RlLGRldil7cmV0dXJuIE1FTUZTLmNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxkZXYpfSxyZW5hbWUob2xkX25vZGUsbmV3X2RpcixuZXdfbmFtZSl7dmFyIG5ld19ub2RlO3RyeXtuZXdfbm9kZT1GUy5sb29rdXBOb2RlKG5ld19kaXIsbmV3X25hbWUpfWNhdGNoKGUpe31pZihuZXdfbm9kZSl7aWYoRlMuaXNEaXIob2xkX25vZGUubW9kZSkpe2Zvcih2YXIgaSBpbiBuZXdfbm9kZS5jb250ZW50cyl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTUpfX1GUy5oYXNoUmVtb3ZlTm9kZShuZXdfbm9kZSl9ZGVsZXRlIG9sZF9ub2RlLnBhcmVudC5jb250ZW50c1tvbGRfbm9kZS5uYW1lXTtuZXdfZGlyLmNvbnRlbnRzW25ld19uYW1lXT1vbGRfbm9kZTtvbGRfbm9kZS5uYW1lPW5ld19uYW1lO25ld19kaXIuY3RpbWU9bmV3X2Rpci5tdGltZT1vbGRfbm9kZS5wYXJlbnQuY3RpbWU9b2xkX25vZGUucGFyZW50Lm10aW1lPURhdGUubm93KCl9LHVubGluayhwYXJlbnQsbmFtZSl7ZGVsZXRlIHBhcmVudC5jb250ZW50c1tuYW1lXTtwYXJlbnQuY3RpbWU9cGFyZW50Lm10aW1lPURhdGUubm93KCl9LHJtZGlyKHBhcmVudCxuYW1lKXt2YXIgbm9kZT1GUy5sb29rdXBOb2RlKHBhcmVudCxuYW1lKTtmb3IodmFyIGkgaW4gbm9kZS5jb250ZW50cyl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTUpfWRlbGV0ZSBwYXJlbnQuY29udGVudHNbbmFtZV07cGFyZW50LmN0aW1lPXBhcmVudC5tdGltZT1EYXRlLm5vdygpfSxyZWFkZGlyKG5vZGUpe3JldHVyblsiLiIsIi4uIiwuLi5PYmplY3Qua2V5cyhub2RlLmNvbnRlbnRzKV19LHN5bWxpbmsocGFyZW50LG5ld25hbWUsb2xkcGF0aCl7dmFyIG5vZGU9TUVNRlMuY3JlYXRlTm9kZShwYXJlbnQsbmV3bmFtZSw1MTF8NDA5NjAsMCk7bm9kZS5saW5rPW9sZHBhdGg7cmV0dXJuIG5vZGV9LHJlYWRsaW5rKG5vZGUpe2lmKCFGUy5pc0xpbmsobm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXJldHVybiBub2RlLmxpbmt9fSxzdHJlYW1fb3BzOntyZWFkKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbil7dmFyIGNvbnRlbnRzPXN0cmVhbS5ub2RlLmNvbnRlbnRzO2lmKHBvc2l0aW9uPj1zdHJlYW0ubm9kZS51c2VkQnl0ZXMpcmV0dXJuIDA7dmFyIHNpemU9TWF0aC5taW4oc3RyZWFtLm5vZGUudXNlZEJ5dGVzLXBvc2l0aW9uLGxlbmd0aCk7YXNzZXJ0KHNpemU+PTApO2lmKHNpemU+OCYmY29udGVudHMuc3ViYXJyYXkpe2J1ZmZlci5zZXQoY29udGVudHMuc3ViYXJyYXkocG9zaXRpb24scG9zaXRpb24rc2l6ZSksb2Zmc2V0KX1lbHNle2Zvcih2YXIgaT0wO2k8c2l6ZTtpKyspYnVmZmVyW29mZnNldCtpXT1jb250ZW50c1twb3NpdGlvbitpXX1yZXR1cm4gc2l6ZX0sd3JpdGUoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uLGNhbk93bil7YXNzZXJ0KCEoYnVmZmVyIGluc3RhbmNlb2YgQXJyYXlCdWZmZXIpKTtpZihidWZmZXIuYnVmZmVyPT09KGdyb3dNZW1WaWV3cygpLEhFQVA4KS5idWZmZXIpe2Nhbk93bj1mYWxzZX1pZighbGVuZ3RoKXJldHVybiAwO3ZhciBub2RlPXN0cmVhbS5ub2RlO25vZGUubXRpbWU9bm9kZS5jdGltZT1EYXRlLm5vdygpO2lmKGJ1ZmZlci5zdWJhcnJheSYmKCFub2RlLmNvbnRlbnRzfHxub2RlLmNvbnRlbnRzLnN1YmFycmF5KSl7aWYoY2FuT3duKXthc3NlcnQocG9zaXRpb249PT0wLCJjYW5Pd24gbXVzdCBpbXBseSBubyB3ZWlyZCBwb3NpdGlvbiBpbnNpZGUgdGhlIGZpbGUiKTtub2RlLmNvbnRlbnRzPWJ1ZmZlci5zdWJhcnJheShvZmZzZXQsb2Zmc2V0K2xlbmd0aCk7bm9kZS51c2VkQnl0ZXM9bGVuZ3RoO3JldHVybiBsZW5ndGh9ZWxzZSBpZihub2RlLnVzZWRCeXRlcz09PTAmJnBvc2l0aW9uPT09MCl7bm9kZS5jb250ZW50cz1idWZmZXIuc2xpY2Uob2Zmc2V0LG9mZnNldCtsZW5ndGgpO25vZGUudXNlZEJ5dGVzPWxlbmd0aDtyZXR1cm4gbGVuZ3RofWVsc2UgaWYocG9zaXRpb24rbGVuZ3RoPD1ub2RlLnVzZWRCeXRlcyl7bm9kZS5jb250ZW50cy5zZXQoYnVmZmVyLnN1YmFycmF5KG9mZnNldCxvZmZzZXQrbGVuZ3RoKSxwb3NpdGlvbik7cmV0dXJuIGxlbmd0aH19TUVNRlMuZXhwYW5kRmlsZVN0b3JhZ2Uobm9kZSxwb3NpdGlvbitsZW5ndGgpO2lmKG5vZGUuY29udGVudHMuc3ViYXJyYXkmJmJ1ZmZlci5zdWJhcnJheSl7bm9kZS5jb250ZW50cy5zZXQoYnVmZmVyLnN1YmFycmF5KG9mZnNldCxvZmZzZXQrbGVuZ3RoKSxwb3NpdGlvbil9ZWxzZXtmb3IodmFyIGk9MDtpPGxlbmd0aDtpKyspe25vZGUuY29udGVudHNbcG9zaXRpb24raV09YnVmZmVyW29mZnNldCtpXX19bm9kZS51c2VkQnl0ZXM9TWF0aC5tYXgobm9kZS51c2VkQnl0ZXMscG9zaXRpb24rbGVuZ3RoKTtyZXR1cm4gbGVuZ3RofSxsbHNlZWsoc3RyZWFtLG9mZnNldCx3aGVuY2Upe3ZhciBwb3NpdGlvbj1vZmZzZXQ7aWYod2hlbmNlPT09MSl7cG9zaXRpb24rPXN0cmVhbS5wb3NpdGlvbn1lbHNlIGlmKHdoZW5jZT09PTIpe2lmKEZTLmlzRmlsZShzdHJlYW0ubm9kZS5tb2RlKSl7cG9zaXRpb24rPXN0cmVhbS5ub2RlLnVzZWRCeXRlc319aWYocG9zaXRpb248MCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXJldHVybiBwb3NpdGlvbn0sbW1hcChzdHJlYW0sbGVuZ3RoLHBvc2l0aW9uLHByb3QsZmxhZ3Mpe2lmKCFGUy5pc0ZpbGUoc3RyZWFtLm5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQzKX12YXIgcHRyO3ZhciBhbGxvY2F0ZWQ7dmFyIGNvbnRlbnRzPXN0cmVhbS5ub2RlLmNvbnRlbnRzO2lmKCEoZmxhZ3MmMikmJmNvbnRlbnRzJiZjb250ZW50cy5idWZmZXI9PT0oZ3Jvd01lbVZpZXdzKCksSEVBUDgpLmJ1ZmZlcil7YWxsb2NhdGVkPWZhbHNlO3B0cj1jb250ZW50cy5ieXRlT2Zmc2V0fWVsc2V7YWxsb2NhdGVkPXRydWU7cHRyPW1tYXBBbGxvYyhsZW5ndGgpO2lmKCFwdHIpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ4KX1pZihjb250ZW50cyl7aWYocG9zaXRpb24+MHx8cG9zaXRpb24rbGVuZ3RoPGNvbnRlbnRzLmxlbmd0aCl7aWYoY29udGVudHMuc3ViYXJyYXkpe2NvbnRlbnRzPWNvbnRlbnRzLnN1YmFycmF5KHBvc2l0aW9uLHBvc2l0aW9uK2xlbmd0aCl9ZWxzZXtjb250ZW50cz1BcnJheS5wcm90b3R5cGUuc2xpY2UuY2FsbChjb250ZW50cyxwb3NpdGlvbixwb3NpdGlvbitsZW5ndGgpfX0oZ3Jvd01lbVZpZXdzKCksSEVBUDgpLnNldChjb250ZW50cyxwdHIpfX1yZXR1cm57cHRyLGFsbG9jYXRlZH19LG1zeW5jKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxtbWFwRmxhZ3Mpe01FTUZTLnN0cmVhbV9vcHMud3JpdGUoc3RyZWFtLGJ1ZmZlciwwLGxlbmd0aCxvZmZzZXQsZmFsc2UpO3JldHVybiAwfX19O3ZhciBGU19tb2RlU3RyaW5nVG9GbGFncz1zdHI9Pnt2YXIgZmxhZ01vZGVzPXtyOjAsInIrIjoyLHc6NTEyfDY0fDEsIncrIjo1MTJ8NjR8MixhOjEwMjR8NjR8MSwiYSsiOjEwMjR8NjR8Mn07dmFyIGZsYWdzPWZsYWdNb2Rlc1tzdHJdO2lmKHR5cGVvZiBmbGFncz09InVuZGVmaW5lZCIpe3Rocm93IG5ldyBFcnJvcihgVW5rbm93biBmaWxlIG9wZW4gbW9kZTogJHtzdHJ9YCl9cmV0dXJuIGZsYWdzfTt2YXIgRlNfZ2V0TW9kZT0oY2FuUmVhZCxjYW5Xcml0ZSk9Pnt2YXIgbW9kZT0wO2lmKGNhblJlYWQpbW9kZXw9MjkyfDczO2lmKGNhbldyaXRlKW1vZGV8PTE0NjtyZXR1cm4gbW9kZX07dmFyIHN0ckVycm9yPWVycm5vPT5VVEY4VG9TdHJpbmcoX3N0cmVycm9yKGVycm5vKSk7dmFyIEVSUk5PX0NPREVTPXtFUEVSTTo2MyxFTk9FTlQ6NDQsRVNSQ0g6NzEsRUlOVFI6MjcsRUlPOjI5LEVOWElPOjYwLEUyQklHOjEsRU5PRVhFQzo0NSxFQkFERjo4LEVDSElMRDoxMixFQUdBSU46NixFV09VTERCTE9DSzo2LEVOT01FTTo0OCxFQUNDRVM6MixFRkFVTFQ6MjEsRU5PVEJMSzoxMDUsRUJVU1k6MTAsRUVYSVNUOjIwLEVYREVWOjc1LEVOT0RFVjo0MyxFTk9URElSOjU0LEVJU0RJUjozMSxFSU5WQUw6MjgsRU5GSUxFOjQxLEVNRklMRTozMyxFTk9UVFk6NTksRVRYVEJTWTo3NCxFRkJJRzoyMixFTk9TUEM6NTEsRVNQSVBFOjcwLEVST0ZTOjY5LEVNTElOSzozNCxFUElQRTo2NCxFRE9NOjE4LEVSQU5HRTo2OCxFTk9NU0c6NDksRUlEUk06MjQsRUNIUk5HOjEwNixFTDJOU1lOQzoxNTYsRUwzSExUOjEwNyxFTDNSU1Q6MTA4LEVMTlJORzoxMDksRVVOQVRDSDoxMTAsRU5PQ1NJOjExMSxFTDJITFQ6MTEyLEVERUFETEs6MTYsRU5PTENLOjQ2LEVCQURFOjExMyxFQkFEUjoxMTQsRVhGVUxMOjExNSxFTk9BTk86MTA0LEVCQURSUUM6MTAzLEVCQURTTFQ6MTAyLEVERUFETE9DSzoxNixFQkZPTlQ6MTAxLEVOT1NUUjoxMDAsRU5PREFUQToxMTYsRVRJTUU6MTE3LEVOT1NSOjExOCxFTk9ORVQ6MTE5LEVOT1BLRzoxMjAsRVJFTU9URToxMjEsRU5PTElOSzo0NyxFQURWOjEyMixFU1JNTlQ6MTIzLEVDT01NOjEyNCxFUFJPVE86NjUsRU1VTFRJSE9QOjM2LEVET1RET1Q6MTI1LEVCQURNU0c6OSxFTk9UVU5JUToxMjYsRUJBREZEOjEyNyxFUkVNQ0hHOjEyOCxFTElCQUNDOjEyOSxFTElCQkFEOjEzMCxFTElCU0NOOjEzMSxFTElCTUFYOjEzMixFTElCRVhFQzoxMzMsRU5PU1lTOjUyLEVOT1RFTVBUWTo1NSxFTkFNRVRPT0xPTkc6MzcsRUxPT1A6MzIsRU9QTk9UU1VQUDoxMzgsRVBGTk9TVVBQT1JUOjEzOSxFQ09OTlJFU0VUOjE1LEVOT0JVRlM6NDIsRUFGTk9TVVBQT1JUOjUsRVBST1RPVFlQRTo2NyxFTk9UU09DSzo1NyxFTk9QUk9UT09QVDo1MCxFU0hVVERPV046MTQwLEVDT05OUkVGVVNFRDoxNCxFQUREUklOVVNFOjMsRUNPTk5BQk9SVEVEOjEzLEVORVRVTlJFQUNIOjQwLEVORVRET1dOOjM4LEVUSU1FRE9VVDo3MyxFSE9TVERPV046MTQyLEVIT1NUVU5SRUFDSDoyMyxFSU5QUk9HUkVTUzoyNixFQUxSRUFEWTo3LEVERVNUQUREUlJFUToxNyxFTVNHU0laRTozNSxFUFJPVE9OT1NVUFBPUlQ6NjYsRVNPQ0tUTk9TVVBQT1JUOjEzNyxFQUREUk5PVEFWQUlMOjQsRU5FVFJFU0VUOjM5LEVJU0NPTk46MzAsRU5PVENPTk46NTMsRVRPT01BTllSRUZTOjE0MSxFVVNFUlM6MTM2LEVEUVVPVDoxOSxFU1RBTEU6NzIsRU5PVFNVUDoxMzgsRU5PTUVESVVNOjE0OCxFSUxTRVE6MjUsRU9WRVJGTE9XOjYxLEVDQU5DRUxFRDoxMSxFTk9UUkVDT1ZFUkFCTEU6NTYsRU9XTkVSREVBRDo2MixFU1RSUElQRToxMzV9O3ZhciBhc3luY0xvYWQ9YXN5bmMgdXJsPT57dmFyIGFycmF5QnVmZmVyPWF3YWl0IHJlYWRBc3luYyh1cmwpO2Fzc2VydChhcnJheUJ1ZmZlcixgTG9hZGluZyBkYXRhIGZpbGUgIiR7dXJsfSIgZmFpbGVkIChubyBhcnJheUJ1ZmZlcikuYCk7cmV0dXJuIG5ldyBVaW50OEFycmF5KGFycmF5QnVmZmVyKX07dmFyIEZTX2NyZWF0ZURhdGFGaWxlPSguLi5hcmdzKT0+RlMuY3JlYXRlRGF0YUZpbGUoLi4uYXJncyk7dmFyIGdldFVuaXF1ZVJ1bkRlcGVuZGVuY3k9aWQ9Pnt2YXIgb3JpZz1pZDt3aGlsZSgxKXtpZighcnVuRGVwZW5kZW5jeVRyYWNraW5nW2lkXSlyZXR1cm4gaWQ7aWQ9b3JpZytNYXRoLnJhbmRvbSgpfX07dmFyIHByZWxvYWRQbHVnaW5zPVtdO3ZhciBGU19oYW5kbGVkQnlQcmVsb2FkUGx1Z2luPWFzeW5jKGJ5dGVBcnJheSxmdWxsbmFtZSk9PntpZih0eXBlb2YgQnJvd3NlciE9InVuZGVmaW5lZCIpQnJvd3Nlci5pbml0KCk7Zm9yKHZhciBwbHVnaW4gb2YgcHJlbG9hZFBsdWdpbnMpe2lmKHBsdWdpblsiY2FuSGFuZGxlIl0oZnVsbG5hbWUpKXthc3NlcnQocGx1Z2luWyJoYW5kbGUiXS5jb25zdHJ1Y3Rvci5uYW1lPT09IkFzeW5jRnVuY3Rpb24iLCJGaWxlc3lzdGVtIHBsdWdpbiBoYW5kbGVycyBtdXN0IGJlIGFzeW5jIGZ1bmN0aW9ucyAoU2VlICMyNDkxNCkiKTtyZXR1cm4gcGx1Z2luWyJoYW5kbGUiXShieXRlQXJyYXksZnVsbG5hbWUpfX1yZXR1cm4gYnl0ZUFycmF5fTt2YXIgRlNfcHJlbG9hZEZpbGU9YXN5bmMocGFyZW50LG5hbWUsdXJsLGNhblJlYWQsY2FuV3JpdGUsZG9udENyZWF0ZUZpbGUsY2FuT3duLHByZUZpbmlzaCk9Pnt2YXIgZnVsbG5hbWU9bmFtZT9QQVRIX0ZTLnJlc29sdmUoUEFUSC5qb2luMihwYXJlbnQsbmFtZSkpOnBhcmVudDt2YXIgZGVwPWdldFVuaXF1ZVJ1bkRlcGVuZGVuY3koYGNwICR7ZnVsbG5hbWV9YCk7YWRkUnVuRGVwZW5kZW5jeShkZXApO3RyeXt2YXIgYnl0ZUFycmF5PXVybDtpZih0eXBlb2YgdXJsPT0ic3RyaW5nIil7Ynl0ZUFycmF5PWF3YWl0IGFzeW5jTG9hZCh1cmwpfWJ5dGVBcnJheT1hd2FpdCBGU19oYW5kbGVkQnlQcmVsb2FkUGx1Z2luKGJ5dGVBcnJheSxmdWxsbmFtZSk7cHJlRmluaXNoPy4oKTtpZighZG9udENyZWF0ZUZpbGUpe0ZTX2NyZWF0ZURhdGFGaWxlKHBhcmVudCxuYW1lLGJ5dGVBcnJheSxjYW5SZWFkLGNhbldyaXRlLGNhbk93bil9fWZpbmFsbHl7cmVtb3ZlUnVuRGVwZW5kZW5jeShkZXApfX07dmFyIEZTX2NyZWF0ZVByZWxvYWRlZEZpbGU9KHBhcmVudCxuYW1lLHVybCxjYW5SZWFkLGNhbldyaXRlLG9ubG9hZCxvbmVycm9yLGRvbnRDcmVhdGVGaWxlLGNhbk93bixwcmVGaW5pc2gpPT57RlNfcHJlbG9hZEZpbGUocGFyZW50LG5hbWUsdXJsLGNhblJlYWQsY2FuV3JpdGUsZG9udENyZWF0ZUZpbGUsY2FuT3duLHByZUZpbmlzaCkudGhlbihvbmxvYWQpLmNhdGNoKG9uZXJyb3IpfTt2YXIgRlM9e3Jvb3Q6bnVsbCxtb3VudHM6W10sZGV2aWNlczp7fSxzdHJlYW1zOltdLG5leHRJbm9kZToxLG5hbWVUYWJsZTpudWxsLGN1cnJlbnRQYXRoOiIvIixpbml0aWFsaXplZDpmYWxzZSxpZ25vcmVQZXJtaXNzaW9uczp0cnVlLGZpbGVzeXN0ZW1zOm51bGwsc3luY0ZTUmVxdWVzdHM6MCxyZWFkRmlsZXM6e30sRXJybm9FcnJvcjpjbGFzcyBleHRlbmRzIEVycm9ye25hbWU9IkVycm5vRXJyb3IiO2NvbnN0cnVjdG9yKGVycm5vKXtzdXBlcihydW50aW1lSW5pdGlhbGl6ZWQ/c3RyRXJyb3IoZXJybm8pOiIiKTt0aGlzLmVycm5vPWVycm5vO2Zvcih2YXIga2V5IGluIEVSUk5PX0NPREVTKXtpZihFUlJOT19DT0RFU1trZXldPT09ZXJybm8pe3RoaXMuY29kZT1rZXk7YnJlYWt9fX19LEZTU3RyZWFtOmNsYXNze3NoYXJlZD17fTtnZXQgb2JqZWN0KCl7cmV0dXJuIHRoaXMubm9kZX1zZXQgb2JqZWN0KHZhbCl7dGhpcy5ub2RlPXZhbH1nZXQgaXNSZWFkKCl7cmV0dXJuKHRoaXMuZmxhZ3MmMjA5NzE1NSkhPT0xfWdldCBpc1dyaXRlKCl7cmV0dXJuKHRoaXMuZmxhZ3MmMjA5NzE1NSkhPT0wfWdldCBpc0FwcGVuZCgpe3JldHVybiB0aGlzLmZsYWdzJjEwMjR9Z2V0IGZsYWdzKCl7cmV0dXJuIHRoaXMuc2hhcmVkLmZsYWdzfXNldCBmbGFncyh2YWwpe3RoaXMuc2hhcmVkLmZsYWdzPXZhbH1nZXQgcG9zaXRpb24oKXtyZXR1cm4gdGhpcy5zaGFyZWQucG9zaXRpb259c2V0IHBvc2l0aW9uKHZhbCl7dGhpcy5zaGFyZWQucG9zaXRpb249dmFsfX0sRlNOb2RlOmNsYXNze25vZGVfb3BzPXt9O3N0cmVhbV9vcHM9e307cmVhZE1vZGU9MjkyfDczO3dyaXRlTW9kZT0xNDY7bW91bnRlZD1udWxsO2NvbnN0cnVjdG9yKHBhcmVudCxuYW1lLG1vZGUscmRldil7aWYoIXBhcmVudCl7cGFyZW50PXRoaXN9dGhpcy5wYXJlbnQ9cGFyZW50O3RoaXMubW91bnQ9cGFyZW50Lm1vdW50O3RoaXMuaWQ9RlMubmV4dElub2RlKys7dGhpcy5uYW1lPW5hbWU7dGhpcy5tb2RlPW1vZGU7dGhpcy5yZGV2PXJkZXY7dGhpcy5hdGltZT10aGlzLm10aW1lPXRoaXMuY3RpbWU9RGF0ZS5ub3coKX1nZXQgcmVhZCgpe3JldHVybih0aGlzLm1vZGUmdGhpcy5yZWFkTW9kZSk9PT10aGlzLnJlYWRNb2RlfXNldCByZWFkKHZhbCl7dmFsP3RoaXMubW9kZXw9dGhpcy5yZWFkTW9kZTp0aGlzLm1vZGUmPX50aGlzLnJlYWRNb2RlfWdldCB3cml0ZSgpe3JldHVybih0aGlzLm1vZGUmdGhpcy53cml0ZU1vZGUpPT09dGhpcy53cml0ZU1vZGV9c2V0IHdyaXRlKHZhbCl7dmFsP3RoaXMubW9kZXw9dGhpcy53cml0ZU1vZGU6dGhpcy5tb2RlJj1+dGhpcy53cml0ZU1vZGV9Z2V0IGlzRm9sZGVyKCl7cmV0dXJuIEZTLmlzRGlyKHRoaXMubW9kZSl9Z2V0IGlzRGV2aWNlKCl7cmV0dXJuIEZTLmlzQ2hyZGV2KHRoaXMubW9kZSl9fSxsb29rdXBQYXRoKHBhdGgsb3B0cz17fSl7aWYoIXBhdGgpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ0KX1vcHRzLmZvbGxvd19tb3VudD8/PXRydWU7aWYoIVBBVEguaXNBYnMocGF0aCkpe3BhdGg9RlMuY3dkKCkrIi8iK3BhdGh9bGlua2xvb3A6Zm9yKHZhciBubGlua3M9MDtubGlua3M8NDA7bmxpbmtzKyspe3ZhciBwYXJ0cz1wYXRoLnNwbGl0KCIvIikuZmlsdGVyKHA9PiEhcCk7dmFyIGN1cnJlbnQ9RlMucm9vdDt2YXIgY3VycmVudF9wYXRoPSIvIjtmb3IodmFyIGk9MDtpPHBhcnRzLmxlbmd0aDtpKyspe3ZhciBpc2xhc3Q9aT09PXBhcnRzLmxlbmd0aC0xO2lmKGlzbGFzdCYmb3B0cy5wYXJlbnQpe2JyZWFrfWlmKHBhcnRzW2ldPT09Ii4iKXtjb250aW51ZX1pZihwYXJ0c1tpXT09PSIuLiIpe2N1cnJlbnRfcGF0aD1QQVRILmRpcm5hbWUoY3VycmVudF9wYXRoKTtpZihGUy5pc1Jvb3QoY3VycmVudCkpe3BhdGg9Y3VycmVudF9wYXRoKyIvIitwYXJ0cy5zbGljZShpKzEpLmpvaW4oIi8iKTtubGlua3MtLTtjb250aW51ZSBsaW5rbG9vcH1lbHNle2N1cnJlbnQ9Y3VycmVudC5wYXJlbnR9Y29udGludWV9Y3VycmVudF9wYXRoPVBBVEguam9pbjIoY3VycmVudF9wYXRoLHBhcnRzW2ldKTt0cnl7Y3VycmVudD1GUy5sb29rdXBOb2RlKGN1cnJlbnQscGFydHNbaV0pfWNhdGNoKGUpe2lmKGU/LmVycm5vPT09NDQmJmlzbGFzdCYmb3B0cy5ub2VudF9va2F5KXtyZXR1cm57cGF0aDpjdXJyZW50X3BhdGh9fXRocm93IGV9aWYoRlMuaXNNb3VudHBvaW50KGN1cnJlbnQpJiYoIWlzbGFzdHx8b3B0cy5mb2xsb3dfbW91bnQpKXtjdXJyZW50PWN1cnJlbnQubW91bnRlZC5yb290fWlmKEZTLmlzTGluayhjdXJyZW50Lm1vZGUpJiYoIWlzbGFzdHx8b3B0cy5mb2xsb3cpKXtpZighY3VycmVudC5ub2RlX29wcy5yZWFkbGluayl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTIpfXZhciBsaW5rPWN1cnJlbnQubm9kZV9vcHMucmVhZGxpbmsoY3VycmVudCk7aWYoIVBBVEguaXNBYnMobGluaykpe2xpbms9UEFUSC5kaXJuYW1lKGN1cnJlbnRfcGF0aCkrIi8iK2xpbmt9cGF0aD1saW5rKyIvIitwYXJ0cy5zbGljZShpKzEpLmpvaW4oIi8iKTtjb250aW51ZSBsaW5rbG9vcH19cmV0dXJue3BhdGg6Y3VycmVudF9wYXRoLG5vZGU6Y3VycmVudH19dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMzIpfSxnZXRQYXRoKG5vZGUpe3ZhciBwYXRoO3doaWxlKHRydWUpe2lmKEZTLmlzUm9vdChub2RlKSl7dmFyIG1vdW50PW5vZGUubW91bnQubW91bnRwb2ludDtpZighcGF0aClyZXR1cm4gbW91bnQ7cmV0dXJuIG1vdW50W21vdW50Lmxlbmd0aC0xXSE9PSIvIj9gJHttb3VudH0vJHtwYXRofWA6bW91bnQrcGF0aH1wYXRoPXBhdGg/YCR7bm9kZS5uYW1lfS8ke3BhdGh9YDpub2RlLm5hbWU7bm9kZT1ub2RlLnBhcmVudH19LGhhc2hOYW1lKHBhcmVudGlkLG5hbWUpe3ZhciBoYXNoPTA7Zm9yKHZhciBpPTA7aTxuYW1lLmxlbmd0aDtpKyspe2hhc2g9KGhhc2g8PDUpLWhhc2grbmFtZS5jaGFyQ29kZUF0KGkpfDB9cmV0dXJuKHBhcmVudGlkK2hhc2g+Pj4wKSVGUy5uYW1lVGFibGUubGVuZ3RofSxoYXNoQWRkTm9kZShub2RlKXt2YXIgaGFzaD1GUy5oYXNoTmFtZShub2RlLnBhcmVudC5pZCxub2RlLm5hbWUpO25vZGUubmFtZV9uZXh0PUZTLm5hbWVUYWJsZVtoYXNoXTtGUy5uYW1lVGFibGVbaGFzaF09bm9kZX0saGFzaFJlbW92ZU5vZGUobm9kZSl7dmFyIGhhc2g9RlMuaGFzaE5hbWUobm9kZS5wYXJlbnQuaWQsbm9kZS5uYW1lKTtpZihGUy5uYW1lVGFibGVbaGFzaF09PT1ub2RlKXtGUy5uYW1lVGFibGVbaGFzaF09bm9kZS5uYW1lX25leHR9ZWxzZXt2YXIgY3VycmVudD1GUy5uYW1lVGFibGVbaGFzaF07d2hpbGUoY3VycmVudCl7aWYoY3VycmVudC5uYW1lX25leHQ9PT1ub2RlKXtjdXJyZW50Lm5hbWVfbmV4dD1ub2RlLm5hbWVfbmV4dDticmVha31jdXJyZW50PWN1cnJlbnQubmFtZV9uZXh0fX19LGxvb2t1cE5vZGUocGFyZW50LG5hbWUpe3ZhciBlcnJDb2RlPUZTLm1heUxvb2t1cChwYXJlbnQpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfXZhciBoYXNoPUZTLmhhc2hOYW1lKHBhcmVudC5pZCxuYW1lKTtmb3IodmFyIG5vZGU9RlMubmFtZVRhYmxlW2hhc2hdO25vZGU7bm9kZT1ub2RlLm5hbWVfbmV4dCl7dmFyIG5vZGVOYW1lPW5vZGUubmFtZTtpZihub2RlLnBhcmVudC5pZD09PXBhcmVudC5pZCYmbm9kZU5hbWU9PT1uYW1lKXtyZXR1cm4gbm9kZX19cmV0dXJuIEZTLmxvb2t1cChwYXJlbnQsbmFtZSl9LGNyZWF0ZU5vZGUocGFyZW50LG5hbWUsbW9kZSxyZGV2KXthc3NlcnQodHlwZW9mIHBhcmVudD09Im9iamVjdCIpO3ZhciBub2RlPW5ldyBGUy5GU05vZGUocGFyZW50LG5hbWUsbW9kZSxyZGV2KTtGUy5oYXNoQWRkTm9kZShub2RlKTtyZXR1cm4gbm9kZX0sZGVzdHJveU5vZGUobm9kZSl7RlMuaGFzaFJlbW92ZU5vZGUobm9kZSl9LGlzUm9vdChub2RlKXtyZXR1cm4gbm9kZT09PW5vZGUucGFyZW50fSxpc01vdW50cG9pbnQobm9kZSl7cmV0dXJuISFub2RlLm1vdW50ZWR9LGlzRmlsZShtb2RlKXtyZXR1cm4obW9kZSY2MTQ0MCk9PT0zMjc2OH0saXNEaXIobW9kZSl7cmV0dXJuKG1vZGUmNjE0NDApPT09MTYzODR9LGlzTGluayhtb2RlKXtyZXR1cm4obW9kZSY2MTQ0MCk9PT00MDk2MH0saXNDaHJkZXYobW9kZSl7cmV0dXJuKG1vZGUmNjE0NDApPT09ODE5Mn0saXNCbGtkZXYobW9kZSl7cmV0dXJuKG1vZGUmNjE0NDApPT09MjQ1NzZ9LGlzRklGTyhtb2RlKXtyZXR1cm4obW9kZSY2MTQ0MCk9PT00MDk2fSxpc1NvY2tldChtb2RlKXtyZXR1cm4obW9kZSY0OTE1Mik9PT00OTE1Mn0sZmxhZ3NUb1Blcm1pc3Npb25TdHJpbmcoZmxhZyl7dmFyIHBlcm1zPVsiciIsInciLCJydyJdW2ZsYWcmM107aWYoZmxhZyY1MTIpe3Blcm1zKz0idyJ9cmV0dXJuIHBlcm1zfSxub2RlUGVybWlzc2lvbnMobm9kZSxwZXJtcyl7aWYoRlMuaWdub3JlUGVybWlzc2lvbnMpe3JldHVybiAwfWlmKHBlcm1zLmluY2x1ZGVzKCJyIikmJiEobm9kZS5tb2RlJjI5Mikpe3JldHVybiAyfWVsc2UgaWYocGVybXMuaW5jbHVkZXMoInciKSYmIShub2RlLm1vZGUmMTQ2KSl7cmV0dXJuIDJ9ZWxzZSBpZihwZXJtcy5pbmNsdWRlcygieCIpJiYhKG5vZGUubW9kZSY3Mykpe3JldHVybiAyfXJldHVybiAwfSxtYXlMb29rdXAoZGlyKXtpZighRlMuaXNEaXIoZGlyLm1vZGUpKXJldHVybiA1NDt2YXIgZXJyQ29kZT1GUy5ub2RlUGVybWlzc2lvbnMoZGlyLCJ4Iik7aWYoZXJyQ29kZSlyZXR1cm4gZXJyQ29kZTtpZighZGlyLm5vZGVfb3BzLmxvb2t1cClyZXR1cm4gMjtyZXR1cm4gMH0sbWF5Q3JlYXRlKGRpcixuYW1lKXtpZighRlMuaXNEaXIoZGlyLm1vZGUpKXtyZXR1cm4gNTR9dHJ5e3ZhciBub2RlPUZTLmxvb2t1cE5vZGUoZGlyLG5hbWUpO3JldHVybiAyMH1jYXRjaChlKXt9cmV0dXJuIEZTLm5vZGVQZXJtaXNzaW9ucyhkaXIsInd4Iil9LG1heURlbGV0ZShkaXIsbmFtZSxpc2Rpcil7dmFyIG5vZGU7dHJ5e25vZGU9RlMubG9va3VwTm9kZShkaXIsbmFtZSl9Y2F0Y2goZSl7cmV0dXJuIGUuZXJybm99dmFyIGVyckNvZGU9RlMubm9kZVBlcm1pc3Npb25zKGRpciwid3giKTtpZihlcnJDb2RlKXtyZXR1cm4gZXJyQ29kZX1pZihpc2Rpcil7aWYoIUZTLmlzRGlyKG5vZGUubW9kZSkpe3JldHVybiA1NH1pZihGUy5pc1Jvb3Qobm9kZSl8fEZTLmdldFBhdGgobm9kZSk9PT1GUy5jd2QoKSl7cmV0dXJuIDEwfX1lbHNle2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe3JldHVybiAzMX19cmV0dXJuIDB9LG1heU9wZW4obm9kZSxmbGFncyl7aWYoIW5vZGUpe3JldHVybiA0NH1pZihGUy5pc0xpbmsobm9kZS5tb2RlKSl7cmV0dXJuIDMyfWVsc2UgaWYoRlMuaXNEaXIobm9kZS5tb2RlKSl7aWYoRlMuZmxhZ3NUb1Blcm1pc3Npb25TdHJpbmcoZmxhZ3MpIT09InIifHxmbGFncyYoNTEyfDY0KSl7cmV0dXJuIDMxfX1yZXR1cm4gRlMubm9kZVBlcm1pc3Npb25zKG5vZGUsRlMuZmxhZ3NUb1Blcm1pc3Npb25TdHJpbmcoZmxhZ3MpKX0sY2hlY2tPcEV4aXN0cyhvcCxlcnIpe2lmKCFvcCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyKX1yZXR1cm4gb3B9LE1BWF9PUEVOX0ZEUzo0MDk2LG5leHRmZCgpe2Zvcih2YXIgZmQ9MDtmZDw9RlMuTUFYX09QRU5fRkRTO2ZkKyspe2lmKCFGUy5zdHJlYW1zW2ZkXSl7cmV0dXJuIGZkfX10aHJvdyBuZXcgRlMuRXJybm9FcnJvcigzMyl9LGdldFN0cmVhbUNoZWNrZWQoZmQpe3ZhciBzdHJlYW09RlMuZ2V0U3RyZWFtKGZkKTtpZighc3RyZWFtKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig4KX1yZXR1cm4gc3RyZWFtfSxnZXRTdHJlYW06ZmQ9PkZTLnN0cmVhbXNbZmRdLGNyZWF0ZVN0cmVhbShzdHJlYW0sZmQ9LTEpe2Fzc2VydChmZD49LTEpO3N0cmVhbT1PYmplY3QuYXNzaWduKG5ldyBGUy5GU1N0cmVhbSxzdHJlYW0pO2lmKGZkPT0tMSl7ZmQ9RlMubmV4dGZkKCl9c3RyZWFtLmZkPWZkO0ZTLnN0cmVhbXNbZmRdPXN0cmVhbTtyZXR1cm4gc3RyZWFtfSxjbG9zZVN0cmVhbShmZCl7RlMuc3RyZWFtc1tmZF09bnVsbH0sZHVwU3RyZWFtKG9yaWdTdHJlYW0sZmQ9LTEpe3ZhciBzdHJlYW09RlMuY3JlYXRlU3RyZWFtKG9yaWdTdHJlYW0sZmQpO3N0cmVhbS5zdHJlYW1fb3BzPy5kdXA/LihzdHJlYW0pO3JldHVybiBzdHJlYW19LGRvU2V0QXR0cihzdHJlYW0sbm9kZSxhdHRyKXt2YXIgc2V0YXR0cj1zdHJlYW0/LnN0cmVhbV9vcHMuc2V0YXR0cjt2YXIgYXJnPXNldGF0dHI/c3RyZWFtOm5vZGU7c2V0YXR0cj8/PW5vZGUubm9kZV9vcHMuc2V0YXR0cjtGUy5jaGVja09wRXhpc3RzKHNldGF0dHIsNjMpO3NldGF0dHIoYXJnLGF0dHIpfSxjaHJkZXZfc3RyZWFtX29wczp7b3BlbihzdHJlYW0pe3ZhciBkZXZpY2U9RlMuZ2V0RGV2aWNlKHN0cmVhbS5ub2RlLnJkZXYpO3N0cmVhbS5zdHJlYW1fb3BzPWRldmljZS5zdHJlYW1fb3BzO3N0cmVhbS5zdHJlYW1fb3BzLm9wZW4/LihzdHJlYW0pfSxsbHNlZWsoKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig3MCl9fSxtYWpvcjpkZXY9PmRldj4+OCxtaW5vcjpkZXY9PmRldiYyNTUsbWFrZWRldjoobWEsbWkpPT5tYTw8OHxtaSxyZWdpc3RlckRldmljZShkZXYsb3BzKXtGUy5kZXZpY2VzW2Rldl09e3N0cmVhbV9vcHM6b3BzfX0sZ2V0RGV2aWNlOmRldj0+RlMuZGV2aWNlc1tkZXZdLGdldE1vdW50cyhtb3VudCl7dmFyIG1vdW50cz1bXTt2YXIgY2hlY2s9W21vdW50XTt3aGlsZShjaGVjay5sZW5ndGgpe3ZhciBtPWNoZWNrLnBvcCgpO21vdW50cy5wdXNoKG0pO2NoZWNrLnB1c2goLi4ubS5tb3VudHMpfXJldHVybiBtb3VudHN9LHN5bmNmcyhwb3B1bGF0ZSxjYWxsYmFjayl7aWYodHlwZW9mIHBvcHVsYXRlPT0iZnVuY3Rpb24iKXtjYWxsYmFjaz1wb3B1bGF0ZTtwb3B1bGF0ZT1mYWxzZX1GUy5zeW5jRlNSZXF1ZXN0cysrO2lmKEZTLnN5bmNGU1JlcXVlc3RzPjEpe2Vycihgd2FybmluZzogJHtGUy5zeW5jRlNSZXF1ZXN0c30gRlMuc3luY2ZzIG9wZXJhdGlvbnMgaW4gZmxpZ2h0IGF0IG9uY2UsIHByb2JhYmx5IGp1c3QgZG9pbmcgZXh0cmEgd29ya2ApfXZhciBtb3VudHM9RlMuZ2V0TW91bnRzKEZTLnJvb3QubW91bnQpO3ZhciBjb21wbGV0ZWQ9MDtmdW5jdGlvbiBkb0NhbGxiYWNrKGVyckNvZGUpe2Fzc2VydChGUy5zeW5jRlNSZXF1ZXN0cz4wKTtGUy5zeW5jRlNSZXF1ZXN0cy0tO3JldHVybiBjYWxsYmFjayhlcnJDb2RlKX1mdW5jdGlvbiBkb25lKGVyckNvZGUpe2lmKGVyckNvZGUpe2lmKCFkb25lLmVycm9yZWQpe2RvbmUuZXJyb3JlZD10cnVlO3JldHVybiBkb0NhbGxiYWNrKGVyckNvZGUpfXJldHVybn1pZigrK2NvbXBsZXRlZD49bW91bnRzLmxlbmd0aCl7ZG9DYWxsYmFjayhudWxsKX19Zm9yKHZhciBtb3VudCBvZiBtb3VudHMpe2lmKG1vdW50LnR5cGUuc3luY2ZzKXttb3VudC50eXBlLnN5bmNmcyhtb3VudCxwb3B1bGF0ZSxkb25lKX1lbHNle2RvbmUobnVsbCl9fX0sbW91bnQodHlwZSxvcHRzLG1vdW50cG9pbnQpe2lmKHR5cGVvZiB0eXBlPT0ic3RyaW5nIil7dGhyb3cgdHlwZX12YXIgcm9vdD1tb3VudHBvaW50PT09Ii8iO3ZhciBwc2V1ZG89IW1vdW50cG9pbnQ7dmFyIG5vZGU7aWYocm9vdCYmRlMucm9vdCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMTApfWVsc2UgaWYoIXJvb3QmJiFwc2V1ZG8pe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChtb3VudHBvaW50LHtmb2xsb3dfbW91bnQ6ZmFsc2V9KTttb3VudHBvaW50PWxvb2t1cC5wYXRoO25vZGU9bG9va3VwLm5vZGU7aWYoRlMuaXNNb3VudHBvaW50KG5vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigxMCl9aWYoIUZTLmlzRGlyKG5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDU0KX19dmFyIG1vdW50PXt0eXBlLG9wdHMsbW91bnRwb2ludCxtb3VudHM6W119O3ZhciBtb3VudFJvb3Q9dHlwZS5tb3VudChtb3VudCk7bW91bnRSb290Lm1vdW50PW1vdW50O21vdW50LnJvb3Q9bW91bnRSb290O2lmKHJvb3Qpe0ZTLnJvb3Q9bW91bnRSb290fWVsc2UgaWYobm9kZSl7bm9kZS5tb3VudGVkPW1vdW50O2lmKG5vZGUubW91bnQpe25vZGUubW91bnQubW91bnRzLnB1c2gobW91bnQpfX1yZXR1cm4gbW91bnRSb290fSx1bm1vdW50KG1vdW50cG9pbnQpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChtb3VudHBvaW50LHtmb2xsb3dfbW91bnQ6ZmFsc2V9KTtpZighRlMuaXNNb3VudHBvaW50KGxvb2t1cC5ub2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXZhciBub2RlPWxvb2t1cC5ub2RlO3ZhciBtb3VudD1ub2RlLm1vdW50ZWQ7dmFyIG1vdW50cz1GUy5nZXRNb3VudHMobW91bnQpO2Zvcih2YXJbaGFzaCxjdXJyZW50XW9mIE9iamVjdC5lbnRyaWVzKEZTLm5hbWVUYWJsZSkpe3doaWxlKGN1cnJlbnQpe3ZhciBuZXh0PWN1cnJlbnQubmFtZV9uZXh0O2lmKG1vdW50cy5pbmNsdWRlcyhjdXJyZW50Lm1vdW50KSl7RlMuZGVzdHJveU5vZGUoY3VycmVudCl9Y3VycmVudD1uZXh0fX1ub2RlLm1vdW50ZWQ9bnVsbDt2YXIgaWR4PW5vZGUubW91bnQubW91bnRzLmluZGV4T2YobW91bnQpO2Fzc2VydChpZHghPT0tMSk7bm9kZS5tb3VudC5tb3VudHMuc3BsaWNlKGlkeCwxKX0sbG9va3VwKHBhcmVudCxuYW1lKXtyZXR1cm4gcGFyZW50Lm5vZGVfb3BzLmxvb2t1cChwYXJlbnQsbmFtZSl9LG1rbm9kKHBhdGgsbW9kZSxkZXYpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtwYXJlbnQ6dHJ1ZX0pO3ZhciBwYXJlbnQ9bG9va3VwLm5vZGU7dmFyIG5hbWU9UEFUSC5iYXNlbmFtZShwYXRoKTtpZighbmFtZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfWlmKG5hbWU9PT0iLiJ8fG5hbWU9PT0iLi4iKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyMCl9dmFyIGVyckNvZGU9RlMubWF5Q3JlYXRlKHBhcmVudCxuYW1lKTtpZihlcnJDb2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcihlcnJDb2RlKX1pZighcGFyZW50Lm5vZGVfb3BzLm1rbm9kKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig2Myl9cmV0dXJuIHBhcmVudC5ub2RlX29wcy5ta25vZChwYXJlbnQsbmFtZSxtb2RlLGRldil9LHN0YXRmcyhwYXRoKXtyZXR1cm4gRlMuc3RhdGZzTm9kZShGUy5sb29rdXBQYXRoKHBhdGgse2ZvbGxvdzp0cnVlfSkubm9kZSl9LHN0YXRmc1N0cmVhbShzdHJlYW0pe3JldHVybiBGUy5zdGF0ZnNOb2RlKHN0cmVhbS5ub2RlKX0sc3RhdGZzTm9kZShub2RlKXt2YXIgcnRuPXtic2l6ZTo0MDk2LGZyc2l6ZTo0MDk2LGJsb2NrczoxZTYsYmZyZWU6NWU1LGJhdmFpbDo1ZTUsZmlsZXM6RlMubmV4dElub2RlLGZmcmVlOkZTLm5leHRJbm9kZS0xLGZzaWQ6NDIsZmxhZ3M6MixuYW1lbGVuOjI1NX07aWYobm9kZS5ub2RlX29wcy5zdGF0ZnMpe09iamVjdC5hc3NpZ24ocnRuLG5vZGUubm9kZV9vcHMuc3RhdGZzKG5vZGUubW91bnQub3B0cy5yb290KSl9cmV0dXJuIHJ0bn0sY3JlYXRlKHBhdGgsbW9kZT00Mzgpe21vZGUmPTQwOTU7bW9kZXw9MzI3Njg7cmV0dXJuIEZTLm1rbm9kKHBhdGgsbW9kZSwwKX0sbWtkaXIocGF0aCxtb2RlPTUxMSl7bW9kZSY9NTExfDUxMjttb2RlfD0xNjM4NDtyZXR1cm4gRlMubWtub2QocGF0aCxtb2RlLDApfSxta2RpclRyZWUocGF0aCxtb2RlKXt2YXIgZGlycz1wYXRoLnNwbGl0KCIvIik7dmFyIGQ9IiI7Zm9yKHZhciBkaXIgb2YgZGlycyl7aWYoIWRpciljb250aW51ZTtpZihkfHxQQVRILmlzQWJzKHBhdGgpKWQrPSIvIjtkKz1kaXI7dHJ5e0ZTLm1rZGlyKGQsbW9kZSl9Y2F0Y2goZSl7aWYoZS5lcnJubyE9MjApdGhyb3cgZX19fSxta2RldihwYXRoLG1vZGUsZGV2KXtpZih0eXBlb2YgZGV2PT0idW5kZWZpbmVkIil7ZGV2PW1vZGU7bW9kZT00Mzh9bW9kZXw9ODE5MjtyZXR1cm4gRlMubWtub2QocGF0aCxtb2RlLGRldil9LHN5bWxpbmsob2xkcGF0aCxuZXdwYXRoKXtpZighUEFUSF9GUy5yZXNvbHZlKG9sZHBhdGgpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKG5ld3BhdGgse3BhcmVudDp0cnVlfSk7dmFyIHBhcmVudD1sb29rdXAubm9kZTtpZighcGFyZW50KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9dmFyIG5ld25hbWU9UEFUSC5iYXNlbmFtZShuZXdwYXRoKTt2YXIgZXJyQ29kZT1GUy5tYXlDcmVhdGUocGFyZW50LG5ld25hbWUpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfWlmKCFwYXJlbnQubm9kZV9vcHMuc3ltbGluayl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNjMpfXJldHVybiBwYXJlbnQubm9kZV9vcHMuc3ltbGluayhwYXJlbnQsbmV3bmFtZSxvbGRwYXRoKX0scmVuYW1lKG9sZF9wYXRoLG5ld19wYXRoKXt2YXIgb2xkX2Rpcm5hbWU9UEFUSC5kaXJuYW1lKG9sZF9wYXRoKTt2YXIgbmV3X2Rpcm5hbWU9UEFUSC5kaXJuYW1lKG5ld19wYXRoKTt2YXIgb2xkX25hbWU9UEFUSC5iYXNlbmFtZShvbGRfcGF0aCk7dmFyIG5ld19uYW1lPVBBVEguYmFzZW5hbWUobmV3X3BhdGgpO3ZhciBsb29rdXAsb2xkX2RpcixuZXdfZGlyO2xvb2t1cD1GUy5sb29rdXBQYXRoKG9sZF9wYXRoLHtwYXJlbnQ6dHJ1ZX0pO29sZF9kaXI9bG9va3VwLm5vZGU7bG9va3VwPUZTLmxvb2t1cFBhdGgobmV3X3BhdGgse3BhcmVudDp0cnVlfSk7bmV3X2Rpcj1sb29rdXAubm9kZTtpZighb2xkX2Rpcnx8IW5ld19kaXIpdGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDQpO2lmKG9sZF9kaXIubW91bnQhPT1uZXdfZGlyLm1vdW50KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig3NSl9dmFyIG9sZF9ub2RlPUZTLmxvb2t1cE5vZGUob2xkX2RpcixvbGRfbmFtZSk7dmFyIHJlbGF0aXZlPVBBVEhfRlMucmVsYXRpdmUob2xkX3BhdGgsbmV3X2Rpcm5hbWUpO2lmKHJlbGF0aXZlLmNoYXJBdCgwKSE9PSIuIil7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfXJlbGF0aXZlPVBBVEhfRlMucmVsYXRpdmUobmV3X3BhdGgsb2xkX2Rpcm5hbWUpO2lmKHJlbGF0aXZlLmNoYXJBdCgwKSE9PSIuIil7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTUpfXZhciBuZXdfbm9kZTt0cnl7bmV3X25vZGU9RlMubG9va3VwTm9kZShuZXdfZGlyLG5ld19uYW1lKX1jYXRjaChlKXt9aWYob2xkX25vZGU9PT1uZXdfbm9kZSl7cmV0dXJufXZhciBpc2Rpcj1GUy5pc0RpcihvbGRfbm9kZS5tb2RlKTt2YXIgZXJyQ29kZT1GUy5tYXlEZWxldGUob2xkX2RpcixvbGRfbmFtZSxpc2Rpcik7aWYoZXJyQ29kZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyQ29kZSl9ZXJyQ29kZT1uZXdfbm9kZT9GUy5tYXlEZWxldGUobmV3X2RpcixuZXdfbmFtZSxpc2Rpcik6RlMubWF5Q3JlYXRlKG5ld19kaXIsbmV3X25hbWUpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfWlmKCFvbGRfZGlyLm5vZGVfb3BzLnJlbmFtZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNjMpfWlmKEZTLmlzTW91bnRwb2ludChvbGRfbm9kZSl8fG5ld19ub2RlJiZGUy5pc01vdW50cG9pbnQobmV3X25vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigxMCl9aWYobmV3X2RpciE9PW9sZF9kaXIpe2VyckNvZGU9RlMubm9kZVBlcm1pc3Npb25zKG9sZF9kaXIsInciKTtpZihlcnJDb2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcihlcnJDb2RlKX19RlMuaGFzaFJlbW92ZU5vZGUob2xkX25vZGUpO3RyeXtvbGRfZGlyLm5vZGVfb3BzLnJlbmFtZShvbGRfbm9kZSxuZXdfZGlyLG5ld19uYW1lKTtvbGRfbm9kZS5wYXJlbnQ9bmV3X2Rpcn1jYXRjaChlKXt0aHJvdyBlfWZpbmFsbHl7RlMuaGFzaEFkZE5vZGUob2xkX25vZGUpfX0scm1kaXIocGF0aCl7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgse3BhcmVudDp0cnVlfSk7dmFyIHBhcmVudD1sb29rdXAubm9kZTt2YXIgbmFtZT1QQVRILmJhc2VuYW1lKHBhdGgpO3ZhciBub2RlPUZTLmxvb2t1cE5vZGUocGFyZW50LG5hbWUpO3ZhciBlcnJDb2RlPUZTLm1heURlbGV0ZShwYXJlbnQsbmFtZSx0cnVlKTtpZihlcnJDb2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcihlcnJDb2RlKX1pZighcGFyZW50Lm5vZGVfb3BzLnJtZGlyKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig2Myl9aWYoRlMuaXNNb3VudHBvaW50KG5vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigxMCl9cGFyZW50Lm5vZGVfb3BzLnJtZGlyKHBhcmVudCxuYW1lKTtGUy5kZXN0cm95Tm9kZShub2RlKX0scmVhZGRpcihwYXRoKXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OnRydWV9KTt2YXIgbm9kZT1sb29rdXAubm9kZTt2YXIgcmVhZGRpcj1GUy5jaGVja09wRXhpc3RzKG5vZGUubm9kZV9vcHMucmVhZGRpciw1NCk7cmV0dXJuIHJlYWRkaXIobm9kZSl9LHVubGluayhwYXRoKXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7cGFyZW50OnRydWV9KTt2YXIgcGFyZW50PWxvb2t1cC5ub2RlO2lmKCFwYXJlbnQpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ0KX12YXIgbmFtZT1QQVRILmJhc2VuYW1lKHBhdGgpO3ZhciBub2RlPUZTLmxvb2t1cE5vZGUocGFyZW50LG5hbWUpO3ZhciBlcnJDb2RlPUZTLm1heURlbGV0ZShwYXJlbnQsbmFtZSxmYWxzZSk7aWYoZXJyQ29kZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyQ29kZSl9aWYoIXBhcmVudC5ub2RlX29wcy51bmxpbmspe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYzKX1pZihGUy5pc01vdW50cG9pbnQobm9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDEwKX1wYXJlbnQubm9kZV9vcHMudW5saW5rKHBhcmVudCxuYW1lKTtGUy5kZXN0cm95Tm9kZShub2RlKX0scmVhZGxpbmsocGF0aCl7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgpO3ZhciBsaW5rPWxvb2t1cC5ub2RlO2lmKCFsaW5rKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9aWYoIWxpbmsubm9kZV9vcHMucmVhZGxpbmspe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI4KX1yZXR1cm4gbGluay5ub2RlX29wcy5yZWFkbGluayhsaW5rKX0sc3RhdChwYXRoLGRvbnRGb2xsb3cpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6IWRvbnRGb2xsb3d9KTt2YXIgbm9kZT1sb29rdXAubm9kZTt2YXIgZ2V0YXR0cj1GUy5jaGVja09wRXhpc3RzKG5vZGUubm9kZV9vcHMuZ2V0YXR0ciw2Myk7cmV0dXJuIGdldGF0dHIobm9kZSl9LGZzdGF0KGZkKXt2YXIgc3RyZWFtPUZTLmdldFN0cmVhbUNoZWNrZWQoZmQpO3ZhciBub2RlPXN0cmVhbS5ub2RlO3ZhciBnZXRhdHRyPXN0cmVhbS5zdHJlYW1fb3BzLmdldGF0dHI7dmFyIGFyZz1nZXRhdHRyP3N0cmVhbTpub2RlO2dldGF0dHI/Pz1ub2RlLm5vZGVfb3BzLmdldGF0dHI7RlMuY2hlY2tPcEV4aXN0cyhnZXRhdHRyLDYzKTtyZXR1cm4gZ2V0YXR0cihhcmcpfSxsc3RhdChwYXRoKXtyZXR1cm4gRlMuc3RhdChwYXRoLHRydWUpfSxkb0NobW9kKHN0cmVhbSxub2RlLG1vZGUsZG9udEZvbGxvdyl7RlMuZG9TZXRBdHRyKHN0cmVhbSxub2RlLHttb2RlOm1vZGUmNDA5NXxub2RlLm1vZGUmfjQwOTUsY3RpbWU6RGF0ZS5ub3coKSxkb250Rm9sbG93fSl9LGNobW9kKHBhdGgsbW9kZSxkb250Rm9sbG93KXt2YXIgbm9kZTtpZih0eXBlb2YgcGF0aD09InN0cmluZyIpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6IWRvbnRGb2xsb3d9KTtub2RlPWxvb2t1cC5ub2RlfWVsc2V7bm9kZT1wYXRofUZTLmRvQ2htb2QobnVsbCxub2RlLG1vZGUsZG9udEZvbGxvdyl9LGxjaG1vZChwYXRoLG1vZGUpe0ZTLmNobW9kKHBhdGgsbW9kZSx0cnVlKX0sZmNobW9kKGZkLG1vZGUpe3ZhciBzdHJlYW09RlMuZ2V0U3RyZWFtQ2hlY2tlZChmZCk7RlMuZG9DaG1vZChzdHJlYW0sc3RyZWFtLm5vZGUsbW9kZSxmYWxzZSl9LGRvQ2hvd24oc3RyZWFtLG5vZGUsZG9udEZvbGxvdyl7RlMuZG9TZXRBdHRyKHN0cmVhbSxub2RlLHt0aW1lc3RhbXA6RGF0ZS5ub3coKSxkb250Rm9sbG93fSl9LGNob3duKHBhdGgsdWlkLGdpZCxkb250Rm9sbG93KXt2YXIgbm9kZTtpZih0eXBlb2YgcGF0aD09InN0cmluZyIpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6IWRvbnRGb2xsb3d9KTtub2RlPWxvb2t1cC5ub2RlfWVsc2V7bm9kZT1wYXRofUZTLmRvQ2hvd24obnVsbCxub2RlLGRvbnRGb2xsb3cpfSxsY2hvd24ocGF0aCx1aWQsZ2lkKXtGUy5jaG93bihwYXRoLHVpZCxnaWQsdHJ1ZSl9LGZjaG93bihmZCx1aWQsZ2lkKXt2YXIgc3RyZWFtPUZTLmdldFN0cmVhbUNoZWNrZWQoZmQpO0ZTLmRvQ2hvd24oc3RyZWFtLHN0cmVhbS5ub2RlLGZhbHNlKX0sZG9UcnVuY2F0ZShzdHJlYW0sbm9kZSxsZW4pe2lmKEZTLmlzRGlyKG5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDMxKX1pZighRlMuaXNGaWxlKG5vZGUubW9kZSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI4KX12YXIgZXJyQ29kZT1GUy5ub2RlUGVybWlzc2lvbnMobm9kZSwidyIpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfUZTLmRvU2V0QXR0cihzdHJlYW0sbm9kZSx7c2l6ZTpsZW4sdGltZXN0YW1wOkRhdGUubm93KCl9KX0sdHJ1bmNhdGUocGF0aCxsZW4pe2lmKGxlbjwwKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9dmFyIG5vZGU7aWYodHlwZW9mIHBhdGg9PSJzdHJpbmciKXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OnRydWV9KTtub2RlPWxvb2t1cC5ub2RlfWVsc2V7bm9kZT1wYXRofUZTLmRvVHJ1bmNhdGUobnVsbCxub2RlLGxlbil9LGZ0cnVuY2F0ZShmZCxsZW4pe3ZhciBzdHJlYW09RlMuZ2V0U3RyZWFtQ2hlY2tlZChmZCk7aWYobGVuPDB8fChzdHJlYW0uZmxhZ3MmMjA5NzE1NSk9PT0wKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9RlMuZG9UcnVuY2F0ZShzdHJlYW0sc3RyZWFtLm5vZGUsbGVuKX0sdXRpbWUocGF0aCxhdGltZSxtdGltZSl7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgse2ZvbGxvdzp0cnVlfSk7dmFyIG5vZGU9bG9va3VwLm5vZGU7dmFyIHNldGF0dHI9RlMuY2hlY2tPcEV4aXN0cyhub2RlLm5vZGVfb3BzLnNldGF0dHIsNjMpO3NldGF0dHIobm9kZSx7YXRpbWUsbXRpbWV9KX0sb3BlbihwYXRoLGZsYWdzLG1vZGU9NDM4KXtpZihwYXRoPT09IiIpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQ0KX1mbGFncz10eXBlb2YgZmxhZ3M9PSJzdHJpbmciP0ZTX21vZGVTdHJpbmdUb0ZsYWdzKGZsYWdzKTpmbGFncztpZihmbGFncyY2NCl7bW9kZT1tb2RlJjQwOTV8MzI3Njh9ZWxzZXttb2RlPTB9dmFyIG5vZGU7dmFyIGlzRGlyUGF0aDtpZih0eXBlb2YgcGF0aD09Im9iamVjdCIpe25vZGU9cGF0aH1lbHNle2lzRGlyUGF0aD1wYXRoLmVuZHNXaXRoKCIvIik7dmFyIGxvb2t1cD1GUy5sb29rdXBQYXRoKHBhdGgse2ZvbGxvdzohKGZsYWdzJjEzMTA3Miksbm9lbnRfb2theTp0cnVlfSk7bm9kZT1sb29rdXAubm9kZTtwYXRoPWxvb2t1cC5wYXRofXZhciBjcmVhdGVkPWZhbHNlO2lmKGZsYWdzJjY0KXtpZihub2RlKXtpZihmbGFncyYxMjgpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDIwKX19ZWxzZSBpZihpc0RpclBhdGgpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDMxKX1lbHNle25vZGU9RlMubWtub2QocGF0aCxtb2RlfDUxMSwwKTtjcmVhdGVkPXRydWV9fWlmKCFub2RlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9aWYoRlMuaXNDaHJkZXYobm9kZS5tb2RlKSl7ZmxhZ3MmPX41MTJ9aWYoZmxhZ3MmNjU1MzYmJiFGUy5pc0Rpcihub2RlLm1vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig1NCl9aWYoIWNyZWF0ZWQpe3ZhciBlcnJDb2RlPUZTLm1heU9wZW4obm9kZSxmbGFncyk7aWYoZXJyQ29kZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoZXJyQ29kZSl9fWlmKGZsYWdzJjUxMiYmIWNyZWF0ZWQpe0ZTLnRydW5jYXRlKG5vZGUsMCl9ZmxhZ3MmPX4oMTI4fDUxMnwxMzEwNzIpO3ZhciBzdHJlYW09RlMuY3JlYXRlU3RyZWFtKHtub2RlLHBhdGg6RlMuZ2V0UGF0aChub2RlKSxmbGFncyxzZWVrYWJsZTp0cnVlLHBvc2l0aW9uOjAsc3RyZWFtX29wczpub2RlLnN0cmVhbV9vcHMsdW5nb3R0ZW46W10sZXJyb3I6ZmFsc2V9KTtpZihzdHJlYW0uc3RyZWFtX29wcy5vcGVuKXtzdHJlYW0uc3RyZWFtX29wcy5vcGVuKHN0cmVhbSl9aWYoY3JlYXRlZCl7RlMuY2htb2Qobm9kZSxtb2RlJjUxMSl9aWYoTW9kdWxlWyJsb2dSZWFkRmlsZXMiXSYmIShmbGFncyYxKSl7aWYoIShwYXRoIGluIEZTLnJlYWRGaWxlcykpe0ZTLnJlYWRGaWxlc1twYXRoXT0xfX1yZXR1cm4gc3RyZWFtfSxjbG9zZShzdHJlYW0pe2lmKEZTLmlzQ2xvc2VkKHN0cmVhbSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKHN0cmVhbS5nZXRkZW50cylzdHJlYW0uZ2V0ZGVudHM9bnVsbDt0cnl7aWYoc3RyZWFtLnN0cmVhbV9vcHMuY2xvc2Upe3N0cmVhbS5zdHJlYW1fb3BzLmNsb3NlKHN0cmVhbSl9fWNhdGNoKGUpe3Rocm93IGV9ZmluYWxseXtGUy5jbG9zZVN0cmVhbShzdHJlYW0uZmQpfXN0cmVhbS5mZD1udWxsfSxpc0Nsb3NlZChzdHJlYW0pe3JldHVybiBzdHJlYW0uZmQ9PT1udWxsfSxsbHNlZWsoc3RyZWFtLG9mZnNldCx3aGVuY2Upe2lmKEZTLmlzQ2xvc2VkKHN0cmVhbSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKCFzdHJlYW0uc2Vla2FibGV8fCFzdHJlYW0uc3RyZWFtX29wcy5sbHNlZWspe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDcwKX1pZih3aGVuY2UhPTAmJndoZW5jZSE9MSYmd2hlbmNlIT0yKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9c3RyZWFtLnBvc2l0aW9uPXN0cmVhbS5zdHJlYW1fb3BzLmxsc2VlayhzdHJlYW0sb2Zmc2V0LHdoZW5jZSk7c3RyZWFtLnVuZ290dGVuPVtdO3JldHVybiBzdHJlYW0ucG9zaXRpb259LHJlYWQoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uKXthc3NlcnQob2Zmc2V0Pj0wKTtpZihsZW5ndGg8MHx8cG9zaXRpb248MCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMjgpfWlmKEZTLmlzQ2xvc2VkKHN0cmVhbSkpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKChzdHJlYW0uZmxhZ3MmMjA5NzE1NSk9PT0xKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig4KX1pZihGUy5pc0RpcihzdHJlYW0ubm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMzEpfWlmKCFzdHJlYW0uc3RyZWFtX29wcy5yZWFkKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9dmFyIHNlZWtpbmc9dHlwZW9mIHBvc2l0aW9uIT0idW5kZWZpbmVkIjtpZighc2Vla2luZyl7cG9zaXRpb249c3RyZWFtLnBvc2l0aW9ufWVsc2UgaWYoIXN0cmVhbS5zZWVrYWJsZSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNzApfXZhciBieXRlc1JlYWQ9c3RyZWFtLnN0cmVhbV9vcHMucmVhZChzdHJlYW0sYnVmZmVyLG9mZnNldCxsZW5ndGgscG9zaXRpb24pO2lmKCFzZWVraW5nKXN0cmVhbS5wb3NpdGlvbis9Ynl0ZXNSZWFkO3JldHVybiBieXRlc1JlYWR9LHdyaXRlKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbixjYW5Pd24pe2Fzc2VydChvZmZzZXQ+PTApO2lmKGxlbmd0aDwwfHxwb3NpdGlvbjwwKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9aWYoRlMuaXNDbG9zZWQoc3RyZWFtKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoOCl9aWYoKHN0cmVhbS5mbGFncyYyMDk3MTU1KT09PTApe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDgpfWlmKEZTLmlzRGlyKHN0cmVhbS5ub2RlLm1vZGUpKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigzMSl9aWYoIXN0cmVhbS5zdHJlYW1fb3BzLndyaXRlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9aWYoc3RyZWFtLnNlZWthYmxlJiZzdHJlYW0uZmxhZ3MmMTAyNCl7RlMubGxzZWVrKHN0cmVhbSwwLDIpfXZhciBzZWVraW5nPXR5cGVvZiBwb3NpdGlvbiE9InVuZGVmaW5lZCI7aWYoIXNlZWtpbmcpe3Bvc2l0aW9uPXN0cmVhbS5wb3NpdGlvbn1lbHNlIGlmKCFzdHJlYW0uc2Vla2FibGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDcwKX12YXIgYnl0ZXNXcml0dGVuPXN0cmVhbS5zdHJlYW1fb3BzLndyaXRlKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbixjYW5Pd24pO2lmKCFzZWVraW5nKXN0cmVhbS5wb3NpdGlvbis9Ynl0ZXNXcml0dGVuO3JldHVybiBieXRlc1dyaXR0ZW59LG1tYXAoc3RyZWFtLGxlbmd0aCxwb3NpdGlvbixwcm90LGZsYWdzKXtpZigocHJvdCYyKSE9PTAmJihmbGFncyYyKT09PTAmJihzdHJlYW0uZmxhZ3MmMjA5NzE1NSkhPT0yKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyKX1pZigoc3RyZWFtLmZsYWdzJjIwOTcxNTUpPT09MSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoMil9aWYoIXN0cmVhbS5zdHJlYW1fb3BzLm1tYXApe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDQzKX1pZighbGVuZ3RoKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOCl9cmV0dXJuIHN0cmVhbS5zdHJlYW1fb3BzLm1tYXAoc3RyZWFtLGxlbmd0aCxwb3NpdGlvbixwcm90LGZsYWdzKX0sbXN5bmMoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLG1tYXBGbGFncyl7YXNzZXJ0KG9mZnNldD49MCk7aWYoIXN0cmVhbS5zdHJlYW1fb3BzLm1zeW5jKXtyZXR1cm4gMH1yZXR1cm4gc3RyZWFtLnN0cmVhbV9vcHMubXN5bmMoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLG1tYXBGbGFncyl9LGlvY3RsKHN0cmVhbSxjbWQsYXJnKXtpZighc3RyZWFtLnN0cmVhbV9vcHMuaW9jdGwpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDU5KX1yZXR1cm4gc3RyZWFtLnN0cmVhbV9vcHMuaW9jdGwoc3RyZWFtLGNtZCxhcmcpfSxyZWFkRmlsZShwYXRoLG9wdHM9e30pe29wdHMuZmxhZ3M9b3B0cy5mbGFnc3x8MDtvcHRzLmVuY29kaW5nPW9wdHMuZW5jb2Rpbmd8fCJiaW5hcnkiO2lmKG9wdHMuZW5jb2RpbmchPT0idXRmOCImJm9wdHMuZW5jb2RpbmchPT0iYmluYXJ5Iil7YWJvcnQoYEludmFsaWQgZW5jb2RpbmcgdHlwZSAiJHtvcHRzLmVuY29kaW5nfSJgKX12YXIgc3RyZWFtPUZTLm9wZW4ocGF0aCxvcHRzLmZsYWdzKTt2YXIgc3RhdD1GUy5zdGF0KHBhdGgpO3ZhciBsZW5ndGg9c3RhdC5zaXplO3ZhciBidWY9bmV3IFVpbnQ4QXJyYXkobGVuZ3RoKTtGUy5yZWFkKHN0cmVhbSxidWYsMCxsZW5ndGgsMCk7aWYob3B0cy5lbmNvZGluZz09PSJ1dGY4Iil7YnVmPVVURjhBcnJheVRvU3RyaW5nKGJ1Zil9RlMuY2xvc2Uoc3RyZWFtKTtyZXR1cm4gYnVmfSx3cml0ZUZpbGUocGF0aCxkYXRhLG9wdHM9e30pe29wdHMuZmxhZ3M9b3B0cy5mbGFnc3x8NTc3O3ZhciBzdHJlYW09RlMub3BlbihwYXRoLG9wdHMuZmxhZ3Msb3B0cy5tb2RlKTtpZih0eXBlb2YgZGF0YT09InN0cmluZyIpe2RhdGE9bmV3IFVpbnQ4QXJyYXkoaW50QXJyYXlGcm9tU3RyaW5nKGRhdGEsdHJ1ZSkpfWlmKEFycmF5QnVmZmVyLmlzVmlldyhkYXRhKSl7RlMud3JpdGUoc3RyZWFtLGRhdGEsMCxkYXRhLmJ5dGVMZW5ndGgsdW5kZWZpbmVkLG9wdHMuY2FuT3duKX1lbHNle2Fib3J0KCJVbnN1cHBvcnRlZCBkYXRhIHR5cGUiKX1GUy5jbG9zZShzdHJlYW0pfSxjd2Q6KCk9PkZTLmN1cnJlbnRQYXRoLGNoZGlyKHBhdGgpe3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtmb2xsb3c6dHJ1ZX0pO2lmKGxvb2t1cC5ub2RlPT09bnVsbCl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDQpfWlmKCFGUy5pc0Rpcihsb29rdXAubm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNTQpfXZhciBlcnJDb2RlPUZTLm5vZGVQZXJtaXNzaW9ucyhsb29rdXAubm9kZSwieCIpO2lmKGVyckNvZGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKGVyckNvZGUpfUZTLmN1cnJlbnRQYXRoPWxvb2t1cC5wYXRofSxjcmVhdGVEZWZhdWx0RGlyZWN0b3JpZXMoKXtGUy5ta2RpcigiL3RtcCIpO0ZTLm1rZGlyKCIvaG9tZSIpO0ZTLm1rZGlyKCIvaG9tZS93ZWJfdXNlciIpfSxjcmVhdGVEZWZhdWx0RGV2aWNlcygpe0ZTLm1rZGlyKCIvZGV2Iik7RlMucmVnaXN0ZXJEZXZpY2UoRlMubWFrZWRldigxLDMpLHtyZWFkOigpPT4wLHdyaXRlOihzdHJlYW0sYnVmZmVyLG9mZnNldCxsZW5ndGgscG9zKT0+bGVuZ3RoLGxsc2VlazooKT0+MH0pO0ZTLm1rZGV2KCIvZGV2L251bGwiLEZTLm1ha2VkZXYoMSwzKSk7VFRZLnJlZ2lzdGVyKEZTLm1ha2VkZXYoNSwwKSxUVFkuZGVmYXVsdF90dHlfb3BzKTtUVFkucmVnaXN0ZXIoRlMubWFrZWRldig2LDApLFRUWS5kZWZhdWx0X3R0eTFfb3BzKTtGUy5ta2RldigiL2Rldi90dHkiLEZTLm1ha2VkZXYoNSwwKSk7RlMubWtkZXYoIi9kZXYvdHR5MSIsRlMubWFrZWRldig2LDApKTt2YXIgcmFuZG9tQnVmZmVyPW5ldyBVaW50OEFycmF5KDEwMjQpLHJhbmRvbUxlZnQ9MDt2YXIgcmFuZG9tQnl0ZT0oKT0+e2lmKHJhbmRvbUxlZnQ9PT0wKXtyYW5kb21GaWxsKHJhbmRvbUJ1ZmZlcik7cmFuZG9tTGVmdD1yYW5kb21CdWZmZXIuYnl0ZUxlbmd0aH1yZXR1cm4gcmFuZG9tQnVmZmVyWy0tcmFuZG9tTGVmdF19O0ZTLmNyZWF0ZURldmljZSgiL2RldiIsInJhbmRvbSIscmFuZG9tQnl0ZSk7RlMuY3JlYXRlRGV2aWNlKCIvZGV2IiwidXJhbmRvbSIscmFuZG9tQnl0ZSk7RlMubWtkaXIoIi9kZXYvc2htIik7RlMubWtkaXIoIi9kZXYvc2htL3RtcCIpfSxjcmVhdGVTcGVjaWFsRGlyZWN0b3JpZXMoKXtGUy5ta2RpcigiL3Byb2MiKTt2YXIgcHJvY19zZWxmPUZTLm1rZGlyKCIvcHJvYy9zZWxmIik7RlMubWtkaXIoIi9wcm9jL3NlbGYvZmQiKTtGUy5tb3VudCh7bW91bnQoKXt2YXIgbm9kZT1GUy5jcmVhdGVOb2RlKHByb2Nfc2VsZiwiZmQiLDE2ODk1LDczKTtub2RlLnN0cmVhbV9vcHM9e2xsc2VlazpNRU1GUy5zdHJlYW1fb3BzLmxsc2Vla307bm9kZS5ub2RlX29wcz17bG9va3VwKHBhcmVudCxuYW1lKXt2YXIgZmQ9K25hbWU7dmFyIHN0cmVhbT1GUy5nZXRTdHJlYW1DaGVja2VkKGZkKTt2YXIgcmV0PXtwYXJlbnQ6bnVsbCxtb3VudDp7bW91bnRwb2ludDoiZmFrZSJ9LG5vZGVfb3BzOntyZWFkbGluazooKT0+c3RyZWFtLnBhdGh9LGlkOmZkKzF9O3JldC5wYXJlbnQ9cmV0O3JldHVybiByZXR9LHJlYWRkaXIoKXtyZXR1cm4gQXJyYXkuZnJvbShGUy5zdHJlYW1zLmVudHJpZXMoKSkuZmlsdGVyKChbayx2XSk9PnYpLm1hcCgoW2ssdl0pPT5rLnRvU3RyaW5nKCkpfX07cmV0dXJuIG5vZGV9fSx7fSwiL3Byb2Mvc2VsZi9mZCIpfSxjcmVhdGVTdGFuZGFyZFN0cmVhbXMoaW5wdXQsb3V0cHV0LGVycm9yKXtpZihpbnB1dCl7RlMuY3JlYXRlRGV2aWNlKCIvZGV2Iiwic3RkaW4iLGlucHV0KX1lbHNle0ZTLnN5bWxpbmsoIi9kZXYvdHR5IiwiL2Rldi9zdGRpbiIpfWlmKG91dHB1dCl7RlMuY3JlYXRlRGV2aWNlKCIvZGV2Iiwic3Rkb3V0IixudWxsLG91dHB1dCl9ZWxzZXtGUy5zeW1saW5rKCIvZGV2L3R0eSIsIi9kZXYvc3Rkb3V0Iil9aWYoZXJyb3Ipe0ZTLmNyZWF0ZURldmljZSgiL2RldiIsInN0ZGVyciIsbnVsbCxlcnJvcil9ZWxzZXtGUy5zeW1saW5rKCIvZGV2L3R0eTEiLCIvZGV2L3N0ZGVyciIpfXZhciBzdGRpbj1GUy5vcGVuKCIvZGV2L3N0ZGluIiwwKTt2YXIgc3Rkb3V0PUZTLm9wZW4oIi9kZXYvc3Rkb3V0IiwxKTt2YXIgc3RkZXJyPUZTLm9wZW4oIi9kZXYvc3RkZXJyIiwxKTthc3NlcnQoc3RkaW4uZmQ9PT0wLGBpbnZhbGlkIGhhbmRsZSBmb3Igc3RkaW4gKCR7c3RkaW4uZmR9KWApO2Fzc2VydChzdGRvdXQuZmQ9PT0xLGBpbnZhbGlkIGhhbmRsZSBmb3Igc3Rkb3V0ICgke3N0ZG91dC5mZH0pYCk7YXNzZXJ0KHN0ZGVyci5mZD09PTIsYGludmFsaWQgaGFuZGxlIGZvciBzdGRlcnIgKCR7c3RkZXJyLmZkfSlgKX0sc3RhdGljSW5pdCgpe0ZTLm5hbWVUYWJsZT1uZXcgQXJyYXkoNDA5Nik7RlMubW91bnQoTUVNRlMse30sIi8iKTtGUy5jcmVhdGVEZWZhdWx0RGlyZWN0b3JpZXMoKTtGUy5jcmVhdGVEZWZhdWx0RGV2aWNlcygpO0ZTLmNyZWF0ZVNwZWNpYWxEaXJlY3RvcmllcygpO0ZTLmZpbGVzeXN0ZW1zPXtNRU1GU319LGluaXQoaW5wdXQsb3V0cHV0LGVycm9yKXthc3NlcnQoIUZTLmluaXRpYWxpemVkLCJGUy5pbml0IHdhcyBwcmV2aW91c2x5IGNhbGxlZC4gSWYgeW91IHdhbnQgdG8gaW5pdGlhbGl6ZSBsYXRlciB3aXRoIGN1c3RvbSBwYXJhbWV0ZXJzLCByZW1vdmUgYW55IGVhcmxpZXIgY2FsbHMgKG5vdGUgdGhhdCBvbmUgaXMgYXV0b21hdGljYWxseSBhZGRlZCB0byB0aGUgZ2VuZXJhdGVkIGNvZGUpIik7RlMuaW5pdGlhbGl6ZWQ9dHJ1ZTtpbnB1dD8/PU1vZHVsZVsic3RkaW4iXTtvdXRwdXQ/Pz1Nb2R1bGVbInN0ZG91dCJdO2Vycm9yPz89TW9kdWxlWyJzdGRlcnIiXTtGUy5jcmVhdGVTdGFuZGFyZFN0cmVhbXMoaW5wdXQsb3V0cHV0LGVycm9yKX0scXVpdCgpe0ZTLmluaXRpYWxpemVkPWZhbHNlO19mZmx1c2goMCk7Zm9yKHZhciBzdHJlYW0gb2YgRlMuc3RyZWFtcyl7aWYoc3RyZWFtKXtGUy5jbG9zZShzdHJlYW0pfX19LGZpbmRPYmplY3QocGF0aCxkb250UmVzb2x2ZUxhc3RMaW5rKXt2YXIgcmV0PUZTLmFuYWx5emVQYXRoKHBhdGgsZG9udFJlc29sdmVMYXN0TGluayk7aWYoIXJldC5leGlzdHMpe3JldHVybiBudWxsfXJldHVybiByZXQub2JqZWN0fSxhbmFseXplUGF0aChwYXRoLGRvbnRSZXNvbHZlTGFzdExpbmspe3RyeXt2YXIgbG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OiFkb250UmVzb2x2ZUxhc3RMaW5rfSk7cGF0aD1sb29rdXAucGF0aH1jYXRjaChlKXt9dmFyIHJldD17aXNSb290OmZhbHNlLGV4aXN0czpmYWxzZSxlcnJvcjowLG5hbWU6bnVsbCxwYXRoOm51bGwsb2JqZWN0Om51bGwscGFyZW50RXhpc3RzOmZhbHNlLHBhcmVudFBhdGg6bnVsbCxwYXJlbnRPYmplY3Q6bnVsbH07dHJ5e3ZhciBsb29rdXA9RlMubG9va3VwUGF0aChwYXRoLHtwYXJlbnQ6dHJ1ZX0pO3JldC5wYXJlbnRFeGlzdHM9dHJ1ZTtyZXQucGFyZW50UGF0aD1sb29rdXAucGF0aDtyZXQucGFyZW50T2JqZWN0PWxvb2t1cC5ub2RlO3JldC5uYW1lPVBBVEguYmFzZW5hbWUocGF0aCk7bG9va3VwPUZTLmxvb2t1cFBhdGgocGF0aCx7Zm9sbG93OiFkb250UmVzb2x2ZUxhc3RMaW5rfSk7cmV0LmV4aXN0cz10cnVlO3JldC5wYXRoPWxvb2t1cC5wYXRoO3JldC5vYmplY3Q9bG9va3VwLm5vZGU7cmV0Lm5hbWU9bG9va3VwLm5vZGUubmFtZTtyZXQuaXNSb290PWxvb2t1cC5wYXRoPT09Ii8ifWNhdGNoKGUpe3JldC5lcnJvcj1lLmVycm5vfXJldHVybiByZXR9LGNyZWF0ZVBhdGgocGFyZW50LHBhdGgsY2FuUmVhZCxjYW5Xcml0ZSl7cGFyZW50PXR5cGVvZiBwYXJlbnQ9PSJzdHJpbmciP3BhcmVudDpGUy5nZXRQYXRoKHBhcmVudCk7dmFyIHBhcnRzPXBhdGguc3BsaXQoIi8iKS5yZXZlcnNlKCk7d2hpbGUocGFydHMubGVuZ3RoKXt2YXIgcGFydD1wYXJ0cy5wb3AoKTtpZighcGFydCljb250aW51ZTt2YXIgY3VycmVudD1QQVRILmpvaW4yKHBhcmVudCxwYXJ0KTt0cnl7RlMubWtkaXIoY3VycmVudCl9Y2F0Y2goZSl7aWYoZS5lcnJubyE9MjApdGhyb3cgZX1wYXJlbnQ9Y3VycmVudH1yZXR1cm4gY3VycmVudH0sY3JlYXRlRmlsZShwYXJlbnQsbmFtZSxwcm9wZXJ0aWVzLGNhblJlYWQsY2FuV3JpdGUpe3ZhciBwYXRoPVBBVEguam9pbjIodHlwZW9mIHBhcmVudD09InN0cmluZyI/cGFyZW50OkZTLmdldFBhdGgocGFyZW50KSxuYW1lKTt2YXIgbW9kZT1GU19nZXRNb2RlKGNhblJlYWQsY2FuV3JpdGUpO3JldHVybiBGUy5jcmVhdGUocGF0aCxtb2RlKX0sY3JlYXRlRGF0YUZpbGUocGFyZW50LG5hbWUsZGF0YSxjYW5SZWFkLGNhbldyaXRlLGNhbk93bil7dmFyIHBhdGg9bmFtZTtpZihwYXJlbnQpe3BhcmVudD10eXBlb2YgcGFyZW50PT0ic3RyaW5nIj9wYXJlbnQ6RlMuZ2V0UGF0aChwYXJlbnQpO3BhdGg9bmFtZT9QQVRILmpvaW4yKHBhcmVudCxuYW1lKTpwYXJlbnR9dmFyIG1vZGU9RlNfZ2V0TW9kZShjYW5SZWFkLGNhbldyaXRlKTt2YXIgbm9kZT1GUy5jcmVhdGUocGF0aCxtb2RlKTtpZihkYXRhKXtpZih0eXBlb2YgZGF0YT09InN0cmluZyIpe3ZhciBhcnI9bmV3IEFycmF5KGRhdGEubGVuZ3RoKTtmb3IodmFyIGk9MCxsZW49ZGF0YS5sZW5ndGg7aTxsZW47KytpKWFycltpXT1kYXRhLmNoYXJDb2RlQXQoaSk7ZGF0YT1hcnJ9RlMuY2htb2Qobm9kZSxtb2RlfDE0Nik7dmFyIHN0cmVhbT1GUy5vcGVuKG5vZGUsNTc3KTtGUy53cml0ZShzdHJlYW0sZGF0YSwwLGRhdGEubGVuZ3RoLDAsY2FuT3duKTtGUy5jbG9zZShzdHJlYW0pO0ZTLmNobW9kKG5vZGUsbW9kZSl9fSxjcmVhdGVEZXZpY2UocGFyZW50LG5hbWUsaW5wdXQsb3V0cHV0KXt2YXIgcGF0aD1QQVRILmpvaW4yKHR5cGVvZiBwYXJlbnQ9PSJzdHJpbmciP3BhcmVudDpGUy5nZXRQYXRoKHBhcmVudCksbmFtZSk7dmFyIG1vZGU9RlNfZ2V0TW9kZSghIWlucHV0LCEhb3V0cHV0KTtGUy5jcmVhdGVEZXZpY2UubWFqb3I/Pz02NDt2YXIgZGV2PUZTLm1ha2VkZXYoRlMuY3JlYXRlRGV2aWNlLm1ham9yKyssMCk7RlMucmVnaXN0ZXJEZXZpY2UoZGV2LHtvcGVuKHN0cmVhbSl7c3RyZWFtLnNlZWthYmxlPWZhbHNlfSxjbG9zZShzdHJlYW0pe2lmKG91dHB1dD8uYnVmZmVyPy5sZW5ndGgpe291dHB1dCgxMCl9fSxyZWFkKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3Mpe3ZhciBieXRlc1JlYWQ9MDtmb3IodmFyIGk9MDtpPGxlbmd0aDtpKyspe3ZhciByZXN1bHQ7dHJ5e3Jlc3VsdD1pbnB1dCgpfWNhdGNoKGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI5KX1pZihyZXN1bHQ9PT11bmRlZmluZWQmJmJ5dGVzUmVhZD09PTApe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDYpfWlmKHJlc3VsdD09PW51bGx8fHJlc3VsdD09PXVuZGVmaW5lZClicmVhaztieXRlc1JlYWQrKztidWZmZXJbb2Zmc2V0K2ldPXJlc3VsdH1pZihieXRlc1JlYWQpe3N0cmVhbS5ub2RlLmF0aW1lPURhdGUubm93KCl9cmV0dXJuIGJ5dGVzUmVhZH0sd3JpdGUoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvcyl7Zm9yKHZhciBpPTA7aTxsZW5ndGg7aSsrKXt0cnl7b3V0cHV0KGJ1ZmZlcltvZmZzZXQraV0pfWNhdGNoKGUpe3Rocm93IG5ldyBGUy5FcnJub0Vycm9yKDI5KX19aWYobGVuZ3RoKXtzdHJlYW0ubm9kZS5tdGltZT1zdHJlYW0ubm9kZS5jdGltZT1EYXRlLm5vdygpfXJldHVybiBpfX0pO3JldHVybiBGUy5ta2RldihwYXRoLG1vZGUsZGV2KX0sZm9yY2VMb2FkRmlsZShvYmope2lmKG9iai5pc0RldmljZXx8b2JqLmlzRm9sZGVyfHxvYmoubGlua3x8b2JqLmNvbnRlbnRzKXJldHVybiB0cnVlO2lmKGdsb2JhbFRoaXMuWE1MSHR0cFJlcXVlc3Qpe2Fib3J0KCJMYXp5IGxvYWRpbmcgc2hvdWxkIGhhdmUgYmVlbiBwZXJmb3JtZWQgKGNvbnRlbnRzIHNldCkgaW4gY3JlYXRlTGF6eUZpbGUsIGJ1dCBpdCB3YXMgbm90LiBMYXp5IGxvYWRpbmcgb25seSB3b3JrcyBpbiB3ZWIgd29ya2Vycy4gVXNlIC0tZW1iZWQtZmlsZSBvciAtLXByZWxvYWQtZmlsZSBpbiBlbWNjIG9uIHRoZSBtYWluIHRocmVhZC4iKX1lbHNle3RyeXtvYmouY29udGVudHM9cmVhZEJpbmFyeShvYmoudXJsKX1jYXRjaChlKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcigyOSl9fX0sY3JlYXRlTGF6eUZpbGUocGFyZW50LG5hbWUsdXJsLGNhblJlYWQsY2FuV3JpdGUpe2NsYXNzIExhenlVaW50OEFycmF5e2xlbmd0aEtub3duPWZhbHNlO2NodW5rcz1bXTtnZXQoaWR4KXtpZihpZHg+dGhpcy5sZW5ndGgtMXx8aWR4PDApe3JldHVybiB1bmRlZmluZWR9dmFyIGNodW5rT2Zmc2V0PWlkeCV0aGlzLmNodW5rU2l6ZTt2YXIgY2h1bmtOdW09aWR4L3RoaXMuY2h1bmtTaXplfDA7cmV0dXJuIHRoaXMuZ2V0dGVyKGNodW5rTnVtKVtjaHVua09mZnNldF19c2V0RGF0YUdldHRlcihnZXR0ZXIpe3RoaXMuZ2V0dGVyPWdldHRlcn1jYWNoZUxlbmd0aCgpe3ZhciB4aHI9bmV3IFhNTEh0dHBSZXF1ZXN0O3hoci5vcGVuKCJIRUFEIix1cmwsZmFsc2UpO3hoci5zZW5kKG51bGwpO2lmKCEoeGhyLnN0YXR1cz49MjAwJiZ4aHIuc3RhdHVzPDMwMHx8eGhyLnN0YXR1cz09PTMwNCkpYWJvcnQoIkNvdWxkbid0IGxvYWQgIit1cmwrIi4gU3RhdHVzOiAiK3hoci5zdGF0dXMpO3ZhciBkYXRhbGVuZ3RoPU51bWJlcih4aHIuZ2V0UmVzcG9uc2VIZWFkZXIoIkNvbnRlbnQtbGVuZ3RoIikpO3ZhciBoZWFkZXI7dmFyIGhhc0J5dGVTZXJ2aW5nPShoZWFkZXI9eGhyLmdldFJlc3BvbnNlSGVhZGVyKCJBY2NlcHQtUmFuZ2VzIikpJiZoZWFkZXI9PT0iYnl0ZXMiO3ZhciB1c2VzR3ppcD0oaGVhZGVyPXhoci5nZXRSZXNwb25zZUhlYWRlcigiQ29udGVudC1FbmNvZGluZyIpKSYmaGVhZGVyPT09Imd6aXAiO3ZhciBjaHVua1NpemU9MTAyNCoxMDI0O2lmKCFoYXNCeXRlU2VydmluZyljaHVua1NpemU9ZGF0YWxlbmd0aDt2YXIgZG9YSFI9KGZyb20sdG8pPT57aWYoZnJvbT50bylhYm9ydCgiaW52YWxpZCByYW5nZSAoIitmcm9tKyIsICIrdG8rIikgb3Igbm8gYnl0ZXMgcmVxdWVzdGVkISIpO2lmKHRvPmRhdGFsZW5ndGgtMSlhYm9ydCgib25seSAiK2RhdGFsZW5ndGgrIiBieXRlcyBhdmFpbGFibGUhIHByb2dyYW1tZXIgZXJyb3IhIik7dmFyIHhocj1uZXcgWE1MSHR0cFJlcXVlc3Q7eGhyLm9wZW4oIkdFVCIsdXJsLGZhbHNlKTtpZihkYXRhbGVuZ3RoIT09Y2h1bmtTaXplKXhoci5zZXRSZXF1ZXN0SGVhZGVyKCJSYW5nZSIsImJ5dGVzPSIrZnJvbSsiLSIrdG8pO3hoci5yZXNwb25zZVR5cGU9ImFycmF5YnVmZmVyIjtpZih4aHIub3ZlcnJpZGVNaW1lVHlwZSl7eGhyLm92ZXJyaWRlTWltZVR5cGUoInRleHQvcGxhaW47IGNoYXJzZXQ9eC11c2VyLWRlZmluZWQiKX14aHIuc2VuZChudWxsKTtpZighKHhoci5zdGF0dXM+PTIwMCYmeGhyLnN0YXR1czwzMDB8fHhoci5zdGF0dXM9PT0zMDQpKWFib3J0KCJDb3VsZG4ndCBsb2FkICIrdXJsKyIuIFN0YXR1czogIit4aHIuc3RhdHVzKTtpZih4aHIucmVzcG9uc2UhPT11bmRlZmluZWQpe3JldHVybiBuZXcgVWludDhBcnJheSh4aHIucmVzcG9uc2V8fFtdKX1yZXR1cm4gaW50QXJyYXlGcm9tU3RyaW5nKHhoci5yZXNwb25zZVRleHR8fCIiLHRydWUpfTt2YXIgbGF6eUFycmF5PXRoaXM7bGF6eUFycmF5LnNldERhdGFHZXR0ZXIoY2h1bmtOdW09Pnt2YXIgc3RhcnQ9Y2h1bmtOdW0qY2h1bmtTaXplO3ZhciBlbmQ9KGNodW5rTnVtKzEpKmNodW5rU2l6ZS0xO2VuZD1NYXRoLm1pbihlbmQsZGF0YWxlbmd0aC0xKTtpZih0eXBlb2YgbGF6eUFycmF5LmNodW5rc1tjaHVua051bV09PSJ1bmRlZmluZWQiKXtsYXp5QXJyYXkuY2h1bmtzW2NodW5rTnVtXT1kb1hIUihzdGFydCxlbmQpfWlmKHR5cGVvZiBsYXp5QXJyYXkuY2h1bmtzW2NodW5rTnVtXT09InVuZGVmaW5lZCIpYWJvcnQoImRvWEhSIGZhaWxlZCEiKTtyZXR1cm4gbGF6eUFycmF5LmNodW5rc1tjaHVua051bV19KTtpZih1c2VzR3ppcHx8IWRhdGFsZW5ndGgpe2NodW5rU2l6ZT1kYXRhbGVuZ3RoPTE7ZGF0YWxlbmd0aD10aGlzLmdldHRlcigwKS5sZW5ndGg7Y2h1bmtTaXplPWRhdGFsZW5ndGg7b3V0KCJMYXp5RmlsZXMgb24gZ3ppcCBmb3JjZXMgZG93bmxvYWQgb2YgdGhlIHdob2xlIGZpbGUgd2hlbiBsZW5ndGggaXMgYWNjZXNzZWQiKX10aGlzLl9sZW5ndGg9ZGF0YWxlbmd0aDt0aGlzLl9jaHVua1NpemU9Y2h1bmtTaXplO3RoaXMubGVuZ3RoS25vd249dHJ1ZX1nZXQgbGVuZ3RoKCl7aWYoIXRoaXMubGVuZ3RoS25vd24pe3RoaXMuY2FjaGVMZW5ndGgoKX1yZXR1cm4gdGhpcy5fbGVuZ3RofWdldCBjaHVua1NpemUoKXtpZighdGhpcy5sZW5ndGhLbm93bil7dGhpcy5jYWNoZUxlbmd0aCgpfXJldHVybiB0aGlzLl9jaHVua1NpemV9fWlmKGdsb2JhbFRoaXMuWE1MSHR0cFJlcXVlc3Qpe2lmKCFFTlZJUk9OTUVOVF9JU19XT1JLRVIpYWJvcnQoIkNhbm5vdCBkbyBzeW5jaHJvbm91cyBiaW5hcnkgWEhScyBvdXRzaWRlIHdlYndvcmtlcnMgaW4gbW9kZXJuIGJyb3dzZXJzLiBVc2UgLS1lbWJlZC1maWxlIG9yIC0tcHJlbG9hZC1maWxlIGluIGVtY2MiKTt2YXIgbGF6eUFycmF5PW5ldyBMYXp5VWludDhBcnJheTt2YXIgcHJvcGVydGllcz17aXNEZXZpY2U6ZmFsc2UsY29udGVudHM6bGF6eUFycmF5fX1lbHNle3ZhciBwcm9wZXJ0aWVzPXtpc0RldmljZTpmYWxzZSx1cmx9fXZhciBub2RlPUZTLmNyZWF0ZUZpbGUocGFyZW50LG5hbWUscHJvcGVydGllcyxjYW5SZWFkLGNhbldyaXRlKTtpZihwcm9wZXJ0aWVzLmNvbnRlbnRzKXtub2RlLmNvbnRlbnRzPXByb3BlcnRpZXMuY29udGVudHN9ZWxzZSBpZihwcm9wZXJ0aWVzLnVybCl7bm9kZS5jb250ZW50cz1udWxsO25vZGUudXJsPXByb3BlcnRpZXMudXJsfU9iamVjdC5kZWZpbmVQcm9wZXJ0aWVzKG5vZGUse3VzZWRCeXRlczp7Z2V0OmZ1bmN0aW9uKCl7cmV0dXJuIHRoaXMuY29udGVudHMubGVuZ3RofX19KTt2YXIgc3RyZWFtX29wcz17fTtmb3IoY29uc3Rba2V5LGZuXW9mIE9iamVjdC5lbnRyaWVzKG5vZGUuc3RyZWFtX29wcykpe3N0cmVhbV9vcHNba2V5XT0oLi4uYXJncyk9PntGUy5mb3JjZUxvYWRGaWxlKG5vZGUpO3JldHVybiBmbiguLi5hcmdzKX19ZnVuY3Rpb24gd3JpdGVDaHVua3Moc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uKXt2YXIgY29udGVudHM9c3RyZWFtLm5vZGUuY29udGVudHM7aWYocG9zaXRpb24+PWNvbnRlbnRzLmxlbmd0aClyZXR1cm4gMDt2YXIgc2l6ZT1NYXRoLm1pbihjb250ZW50cy5sZW5ndGgtcG9zaXRpb24sbGVuZ3RoKTthc3NlcnQoc2l6ZT49MCk7aWYoY29udGVudHMuc2xpY2Upe2Zvcih2YXIgaT0wO2k8c2l6ZTtpKyspe2J1ZmZlcltvZmZzZXQraV09Y29udGVudHNbcG9zaXRpb24raV19fWVsc2V7Zm9yKHZhciBpPTA7aTxzaXplO2krKyl7YnVmZmVyW29mZnNldCtpXT1jb250ZW50cy5nZXQocG9zaXRpb24raSl9fXJldHVybiBzaXplfXN0cmVhbV9vcHMucmVhZD0oc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuZ3RoLHBvc2l0aW9uKT0+e0ZTLmZvcmNlTG9hZEZpbGUobm9kZSk7cmV0dXJuIHdyaXRlQ2h1bmtzKHN0cmVhbSxidWZmZXIsb2Zmc2V0LGxlbmd0aCxwb3NpdGlvbil9O3N0cmVhbV9vcHMubW1hcD0oc3RyZWFtLGxlbmd0aCxwb3NpdGlvbixwcm90LGZsYWdzKT0+e0ZTLmZvcmNlTG9hZEZpbGUobm9kZSk7dmFyIHB0cj1tbWFwQWxsb2MobGVuZ3RoKTtpZighcHRyKXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0OCl9d3JpdGVDaHVua3Moc3RyZWFtLChncm93TWVtVmlld3MoKSxIRUFQOCkscHRyLGxlbmd0aCxwb3NpdGlvbik7cmV0dXJue3B0cixhbGxvY2F0ZWQ6dHJ1ZX19O25vZGUuc3RyZWFtX29wcz1zdHJlYW1fb3BzO3JldHVybiBub2RlfSxhYnNvbHV0ZVBhdGgoKXthYm9ydCgiRlMuYWJzb2x1dGVQYXRoIGhhcyBiZWVuIHJlbW92ZWQ7IHVzZSBQQVRIX0ZTLnJlc29sdmUgaW5zdGVhZCIpfSxjcmVhdGVGb2xkZXIoKXthYm9ydCgiRlMuY3JlYXRlRm9sZGVyIGhhcyBiZWVuIHJlbW92ZWQ7IHVzZSBGUy5ta2RpciBpbnN0ZWFkIil9LGNyZWF0ZUxpbmsoKXthYm9ydCgiRlMuY3JlYXRlTGluayBoYXMgYmVlbiByZW1vdmVkOyB1c2UgRlMuc3ltbGluayBpbnN0ZWFkIil9LGpvaW5QYXRoKCl7YWJvcnQoIkZTLmpvaW5QYXRoIGhhcyBiZWVuIHJlbW92ZWQ7IHVzZSBQQVRILmpvaW4gaW5zdGVhZCIpfSxtbWFwQWxsb2MoKXthYm9ydCgiRlMubW1hcEFsbG9jIGhhcyBiZWVuIHJlcGxhY2VkIGJ5IHRoZSB0b3AgbGV2ZWwgZnVuY3Rpb24gbW1hcEFsbG9jIil9LHN0YW5kYXJkaXplUGF0aCgpe2Fib3J0KCJGUy5zdGFuZGFyZGl6ZVBhdGggaGFzIGJlZW4gcmVtb3ZlZDsgdXNlIFBBVEgubm9ybWFsaXplIGluc3RlYWQiKX19O3ZhciBTWVNDQUxMUz17REVGQVVMVF9QT0xMTUFTSzo1LGNhbGN1bGF0ZUF0KGRpcmZkLHBhdGgsYWxsb3dFbXB0eSl7aWYoUEFUSC5pc0FicyhwYXRoKSl7cmV0dXJuIHBhdGh9dmFyIGRpcjtpZihkaXJmZD09PS0xMDApe2Rpcj1GUy5jd2QoKX1lbHNle3ZhciBkaXJzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGRpcmZkKTtkaXI9ZGlyc3RyZWFtLnBhdGh9aWYocGF0aC5sZW5ndGg9PTApe2lmKCFhbGxvd0VtcHR5KXt0aHJvdyBuZXcgRlMuRXJybm9FcnJvcig0NCl9cmV0dXJuIGRpcn1yZXR1cm4gZGlyKyIvIitwYXRofSx3cml0ZVN0YXQoYnVmLHN0YXQpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWY+PjJdPXN0YXQuZGV2Oyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrND4+Ml09c3RhdC5tb2RlOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrOD4+Ml09c3RhdC5ubGluazsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzEyPj4yXT1zdGF0LnVpZDsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzE2Pj4yXT1zdGF0LmdpZDsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzIwPj4yXT1zdGF0LnJkZXY7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzI0Pj4zXT1CaWdJbnQoc3RhdC5zaXplKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVtidWYrMzI+PjJdPTQwOTY7KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYnVmKzM2Pj4yXT1zdGF0LmJsb2Nrczt2YXIgYXRpbWU9c3RhdC5hdGltZS5nZXRUaW1lKCk7dmFyIG10aW1lPXN0YXQubXRpbWUuZ2V0VGltZSgpO3ZhciBjdGltZT1zdGF0LmN0aW1lLmdldFRpbWUoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtidWYrNDA+PjNdPUJpZ0ludChNYXRoLmZsb29yKGF0aW1lLzFlMykpOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrNDg+PjJdPWF0aW1lJTFlMyoxZTMqMWUzOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1Zis1Nj4+M109QmlnSW50KE1hdGguZmxvb3IobXRpbWUvMWUzKSk7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2J1Zis2ND4+Ml09bXRpbWUlMWUzKjFlMyoxZTM7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzcyPj4zXT1CaWdJbnQoTWF0aC5mbG9vcihjdGltZS8xZTMpKTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzgwPj4yXT1jdGltZSUxZTMqMWUzKjFlMzsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtidWYrODg+PjNdPUJpZ0ludChzdGF0Lmlubyk7cmV0dXJuIDB9LHdyaXRlU3RhdEZzKGJ1ZixzdGF0cyl7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2J1Zis0Pj4yXT1zdGF0cy5ic2l6ZTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzYwPj4yXT1zdGF0cy5ic2l6ZTsoZ3Jvd01lbVZpZXdzKCksSEVBUDY0KVtidWYrOD4+M109QmlnSW50KHN0YXRzLmJsb2Nrcyk7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzE2Pj4zXT1CaWdJbnQoc3RhdHMuYmZyZWUpOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1ZisyND4+M109QmlnSW50KHN0YXRzLmJhdmFpbCk7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmKzMyPj4zXT1CaWdJbnQoc3RhdHMuZmlsZXMpOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW2J1Zis0MD4+M109QmlnSW50KHN0YXRzLmZmcmVlKTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbYnVmKzQ4Pj4yXT1zdGF0cy5mc2lkOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrNjQ+PjJdPXN0YXRzLmZsYWdzOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWYrNTY+PjJdPXN0YXRzLm5hbWVsZW59LGRvTXN5bmMoYWRkcixzdHJlYW0sbGVuLGZsYWdzLG9mZnNldCl7aWYoIUZTLmlzRmlsZShzdHJlYW0ubm9kZS5tb2RlKSl7dGhyb3cgbmV3IEZTLkVycm5vRXJyb3IoNDMpfWlmKGZsYWdzJjIpe3JldHVybiAwfXZhciBidWZmZXI9KGdyb3dNZW1WaWV3cygpLEhFQVBVOCkuc2xpY2UoYWRkcixhZGRyK2xlbik7RlMubXN5bmMoc3RyZWFtLGJ1ZmZlcixvZmZzZXQsbGVuLGZsYWdzKX0sZ2V0U3RyZWFtRnJvbUZEKGZkKXt2YXIgc3RyZWFtPUZTLmdldFN0cmVhbUNoZWNrZWQoZmQpO3JldHVybiBzdHJlYW19LHZhcmFyZ3M6dW5kZWZpbmVkLGdldFN0cihwdHIpe3ZhciByZXQ9VVRGOFRvU3RyaW5nKHB0cik7cmV0dXJuIHJldH19O2Z1bmN0aW9uIF9fX3N5c2NhbGxfZmNudGw2NChmZCxjbWQsdmFyYXJncyl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMywwLDEsZmQsY21kLHZhcmFyZ3MpO1NZU0NBTExTLnZhcmFyZ3M9dmFyYXJnczt0cnl7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO3N3aXRjaChjbWQpe2Nhc2UgMDp7dmFyIGFyZz1zeXNjYWxsR2V0VmFyYXJnSSgpO2lmKGFyZzwwKXtyZXR1cm4tMjh9d2hpbGUoRlMuc3RyZWFtc1thcmddKXthcmcrK312YXIgbmV3U3RyZWFtO25ld1N0cmVhbT1GUy5kdXBTdHJlYW0oc3RyZWFtLGFyZyk7cmV0dXJuIG5ld1N0cmVhbS5mZH1jYXNlIDE6Y2FzZSAyOnJldHVybiAwO2Nhc2UgMzpyZXR1cm4gc3RyZWFtLmZsYWdzO2Nhc2UgNDp7dmFyIGFyZz1zeXNjYWxsR2V0VmFyYXJnSSgpO3N0cmVhbS5mbGFnc3w9YXJnO3JldHVybiAwfWNhc2UgMTI6e3ZhciBhcmc9c3lzY2FsbEdldFZhcmFyZ1AoKTt2YXIgb2Zmc2V0PTA7KGdyb3dNZW1WaWV3cygpLEhFQVAxNilbYXJnK29mZnNldD4+MV09MjtyZXR1cm4gMH1jYXNlIDEzOmNhc2UgMTQ6cmV0dXJuIDB9cmV0dXJuLTI4fWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX19zeXNjYWxsX2ZzdGF0NjQoZmQsYnVmKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg0LDAsMSxmZCxidWYpO3RyeXtyZXR1cm4gU1lTQ0FMTFMud3JpdGVTdGF0KGJ1ZixGUy5mc3RhdChmZCkpfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX19zeXNjYWxsX2lvY3RsKGZkLG9wLHZhcmFyZ3Mpe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDUsMCwxLGZkLG9wLHZhcmFyZ3MpO1NZU0NBTExTLnZhcmFyZ3M9dmFyYXJnczt0cnl7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO3N3aXRjaChvcCl7Y2FzZSAyMTUwOTp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O3JldHVybiAwfWNhc2UgMjE1MDU6e2lmKCFzdHJlYW0udHR5KXJldHVybi01OTtpZihzdHJlYW0udHR5Lm9wcy5pb2N0bF90Y2dldHMpe3ZhciB0ZXJtaW9zPXN0cmVhbS50dHkub3BzLmlvY3RsX3RjZ2V0cyhzdHJlYW0pO3ZhciBhcmdwPXN5c2NhbGxHZXRWYXJhcmdQKCk7KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYXJncD4+Ml09dGVybWlvcy5jX2lmbGFnfHwwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArND4+Ml09dGVybWlvcy5jX29mbGFnfHwwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArOD4+Ml09dGVybWlvcy5jX2NmbGFnfHwwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArMTI+PjJdPXRlcm1pb3MuY19sZmxhZ3x8MDtmb3IodmFyIGk9MDtpPDMyO2krKyl7KGdyb3dNZW1WaWV3cygpLEhFQVA4KVthcmdwK2krMTddPXRlcm1pb3MuY19jY1tpXXx8MH1yZXR1cm4gMH1yZXR1cm4gMH1jYXNlIDIxNTEwOmNhc2UgMjE1MTE6Y2FzZSAyMTUxMjp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O3JldHVybiAwfWNhc2UgMjE1MDY6Y2FzZSAyMTUwNzpjYXNlIDIxNTA4OntpZighc3RyZWFtLnR0eSlyZXR1cm4tNTk7aWYoc3RyZWFtLnR0eS5vcHMuaW9jdGxfdGNzZXRzKXt2YXIgYXJncD1zeXNjYWxsR2V0VmFyYXJnUCgpO3ZhciBjX2lmbGFnPShncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3A+PjJdO3ZhciBjX29mbGFnPShncm93TWVtVmlld3MoKSxIRUFQMzIpW2FyZ3ArND4+Ml07dmFyIGNfY2ZsYWc9KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYXJncCs4Pj4yXTt2YXIgY19sZmxhZz0oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVthcmdwKzEyPj4yXTt2YXIgY19jYz1bXTtmb3IodmFyIGk9MDtpPDMyO2krKyl7Y19jYy5wdXNoKChncm93TWVtVmlld3MoKSxIRUFQOClbYXJncCtpKzE3XSl9cmV0dXJuIHN0cmVhbS50dHkub3BzLmlvY3RsX3Rjc2V0cyhzdHJlYW0udHR5LG9wLHtjX2lmbGFnLGNfb2ZsYWcsY19jZmxhZyxjX2xmbGFnLGNfY2N9KX1yZXR1cm4gMH1jYXNlIDIxNTE5OntpZighc3RyZWFtLnR0eSlyZXR1cm4tNTk7dmFyIGFyZ3A9c3lzY2FsbEdldFZhcmFyZ1AoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVthcmdwPj4yXT0wO3JldHVybiAwfWNhc2UgMjE1MjA6e2lmKCFzdHJlYW0udHR5KXJldHVybi01OTtyZXR1cm4tMjh9Y2FzZSAyMTUzNzpjYXNlIDIxNTMxOnt2YXIgYXJncD1zeXNjYWxsR2V0VmFyYXJnUCgpO3JldHVybiBGUy5pb2N0bChzdHJlYW0sb3AsYXJncCl9Y2FzZSAyMTUyMzp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O2lmKHN0cmVhbS50dHkub3BzLmlvY3RsX3Rpb2Nnd2luc3ope3ZhciB3aW5zaXplPXN0cmVhbS50dHkub3BzLmlvY3RsX3Rpb2Nnd2luc3ooc3RyZWFtLnR0eSk7dmFyIGFyZ3A9c3lzY2FsbEdldFZhcmFyZ1AoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUDE2KVthcmdwPj4xXT13aW5zaXplWzBdOyhncm93TWVtVmlld3MoKSxIRUFQMTYpW2FyZ3ArMj4+MV09d2luc2l6ZVsxXX1yZXR1cm4gMH1jYXNlIDIxNTI0OntpZighc3RyZWFtLnR0eSlyZXR1cm4tNTk7cmV0dXJuIDB9Y2FzZSAyMTUxNTp7aWYoIXN0cmVhbS50dHkpcmV0dXJuLTU5O3JldHVybiAwfWRlZmF1bHQ6cmV0dXJuLTI4fX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319ZnVuY3Rpb24gX19fc3lzY2FsbF9sc3RhdDY0KHBhdGgsYnVmKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg2LDAsMSxwYXRoLGJ1Zik7dHJ5e3BhdGg9U1lTQ0FMTFMuZ2V0U3RyKHBhdGgpO3JldHVybiBTWVNDQUxMUy53cml0ZVN0YXQoYnVmLEZTLmxzdGF0KHBhdGgpKX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319ZnVuY3Rpb24gX19fc3lzY2FsbF9uZXdmc3RhdGF0KGRpcmZkLHBhdGgsYnVmLGZsYWdzKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg3LDAsMSxkaXJmZCxwYXRoLGJ1ZixmbGFncyk7dHJ5e3BhdGg9U1lTQ0FMTFMuZ2V0U3RyKHBhdGgpO3ZhciBub2ZvbGxvdz1mbGFncyYyNTY7dmFyIGFsbG93RW1wdHk9ZmxhZ3MmNDA5NjtmbGFncz1mbGFncyZ+NjQwMDthc3NlcnQoIWZsYWdzLGB1bmtub3duIGZsYWdzIGluIF9fc3lzY2FsbF9uZXdmc3RhdGF0OiAke2ZsYWdzfWApO3BhdGg9U1lTQ0FMTFMuY2FsY3VsYXRlQXQoZGlyZmQscGF0aCxhbGxvd0VtcHR5KTtyZXR1cm4gU1lTQ0FMTFMud3JpdGVTdGF0KGJ1Zixub2ZvbGxvdz9GUy5sc3RhdChwYXRoKTpGUy5zdGF0KHBhdGgpKX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319ZnVuY3Rpb24gX19fc3lzY2FsbF9vcGVuYXQoZGlyZmQscGF0aCxmbGFncyx2YXJhcmdzKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCg4LDAsMSxkaXJmZCxwYXRoLGZsYWdzLHZhcmFyZ3MpO1NZU0NBTExTLnZhcmFyZ3M9dmFyYXJnczt0cnl7cGF0aD1TWVNDQUxMUy5nZXRTdHIocGF0aCk7cGF0aD1TWVNDQUxMUy5jYWxjdWxhdGVBdChkaXJmZCxwYXRoKTt2YXIgbW9kZT12YXJhcmdzP3N5c2NhbGxHZXRWYXJhcmdJKCk6MDtyZXR1cm4gRlMub3BlbihwYXRoLGZsYWdzLG1vZGUpLmZkfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybi1lLmVycm5vfX1mdW5jdGlvbiBfX19zeXNjYWxsX3N0YXQ2NChwYXRoLGJ1Zil7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoOSwwLDEscGF0aCxidWYpO3RyeXtwYXRoPVNZU0NBTExTLmdldFN0cihwYXRoKTtyZXR1cm4gU1lTQ0FMTFMud3JpdGVTdGF0KGJ1ZixGUy5zdGF0KHBhdGgpKX1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4tZS5lcnJub319dmFyIF9fYWJvcnRfanM9KCk9PmFib3J0KCJuYXRpdmUgY29kZSBjYWxsZWQgYWJvcnQoKSIpO3ZhciBBc2NpaVRvU3RyaW5nPXB0cj0+e3ZhciBzdHI9IiI7d2hpbGUoMSl7dmFyIGNoPShncm93TWVtVmlld3MoKSxIRUFQVTgpW3B0cisrXTtpZighY2gpcmV0dXJuIHN0cjtzdHIrPVN0cmluZy5mcm9tQ2hhckNvZGUoY2gpfX07dmFyIGF3YWl0aW5nRGVwZW5kZW5jaWVzPXt9O3ZhciByZWdpc3RlcmVkVHlwZXM9e307dmFyIHR5cGVEZXBlbmRlbmNpZXM9e307dmFyIEJpbmRpbmdFcnJvcj1jbGFzcyBCaW5kaW5nRXJyb3IgZXh0ZW5kcyBFcnJvcntjb25zdHJ1Y3RvcihtZXNzYWdlKXtzdXBlcihtZXNzYWdlKTt0aGlzLm5hbWU9IkJpbmRpbmdFcnJvciJ9fTt2YXIgdGhyb3dCaW5kaW5nRXJyb3I9bWVzc2FnZT0+e3Rocm93IG5ldyBCaW5kaW5nRXJyb3IobWVzc2FnZSl9O2Z1bmN0aW9uIHNoYXJlZFJlZ2lzdGVyVHlwZShyYXdUeXBlLHJlZ2lzdGVyZWRJbnN0YW5jZSxvcHRpb25zPXt9KXt2YXIgbmFtZT1yZWdpc3RlcmVkSW5zdGFuY2UubmFtZTtpZighcmF3VHlwZSl7dGhyb3dCaW5kaW5nRXJyb3IoYHR5cGUgIiR7bmFtZX0iIG11c3QgaGF2ZSBhIHBvc2l0aXZlIGludGVnZXIgdHlwZWlkIHBvaW50ZXJgKX1pZihyZWdpc3RlcmVkVHlwZXMuaGFzT3duUHJvcGVydHkocmF3VHlwZSkpe2lmKG9wdGlvbnMuaWdub3JlRHVwbGljYXRlUmVnaXN0cmF0aW9ucyl7cmV0dXJufWVsc2V7dGhyb3dCaW5kaW5nRXJyb3IoYENhbm5vdCByZWdpc3RlciB0eXBlICcke25hbWV9JyB0d2ljZWApfX1yZWdpc3RlcmVkVHlwZXNbcmF3VHlwZV09cmVnaXN0ZXJlZEluc3RhbmNlO2RlbGV0ZSB0eXBlRGVwZW5kZW5jaWVzW3Jhd1R5cGVdO2lmKGF3YWl0aW5nRGVwZW5kZW5jaWVzLmhhc093blByb3BlcnR5KHJhd1R5cGUpKXt2YXIgY2FsbGJhY2tzPWF3YWl0aW5nRGVwZW5kZW5jaWVzW3Jhd1R5cGVdO2RlbGV0ZSBhd2FpdGluZ0RlcGVuZGVuY2llc1tyYXdUeXBlXTtjYWxsYmFja3MuZm9yRWFjaChjYj0+Y2IoKSl9fWZ1bmN0aW9uIHJlZ2lzdGVyVHlwZShyYXdUeXBlLHJlZ2lzdGVyZWRJbnN0YW5jZSxvcHRpb25zPXt9KXtyZXR1cm4gc2hhcmVkUmVnaXN0ZXJUeXBlKHJhd1R5cGUscmVnaXN0ZXJlZEluc3RhbmNlLG9wdGlvbnMpfXZhciBpbnRlZ2VyUmVhZFZhbHVlRnJvbVBvaW50ZXI9KG5hbWUsd2lkdGgsc2lnbmVkKT0+e3N3aXRjaCh3aWR0aCl7Y2FzZSAxOnJldHVybiBzaWduZWQ/cG9pbnRlcj0+KGdyb3dNZW1WaWV3cygpLEhFQVA4KVtwb2ludGVyXTpwb2ludGVyPT4oZ3Jvd01lbVZpZXdzKCksSEVBUFU4KVtwb2ludGVyXTtjYXNlIDI6cmV0dXJuIHNpZ25lZD9wb2ludGVyPT4oZ3Jvd01lbVZpZXdzKCksSEVBUDE2KVtwb2ludGVyPj4xXTpwb2ludGVyPT4oZ3Jvd01lbVZpZXdzKCksSEVBUFUxNilbcG9pbnRlcj4+MV07Y2FzZSA0OnJldHVybiBzaWduZWQ/cG9pbnRlcj0+KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbcG9pbnRlcj4+Ml06cG9pbnRlcj0+KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3BvaW50ZXI+PjJdO2Nhc2UgODpyZXR1cm4gc2lnbmVkP3BvaW50ZXI9Pihncm93TWVtVmlld3MoKSxIRUFQNjQpW3BvaW50ZXI+PjNdOnBvaW50ZXI9Pihncm93TWVtVmlld3MoKSxIRUFQVTY0KVtwb2ludGVyPj4zXTtkZWZhdWx0OnRocm93IG5ldyBUeXBlRXJyb3IoYGludmFsaWQgaW50ZWdlciB3aWR0aCAoJHt3aWR0aH0pOiAke25hbWV9YCl9fTt2YXIgZW1iaW5kUmVwcj12PT57aWYodj09PW51bGwpe3JldHVybiJudWxsIn12YXIgdD10eXBlb2YgdjtpZih0PT09Im9iamVjdCJ8fHQ9PT0iYXJyYXkifHx0PT09ImZ1bmN0aW9uIil7cmV0dXJuIHYudG9TdHJpbmcoKX1lbHNle3JldHVybiIiK3Z9fTt2YXIgYXNzZXJ0SW50ZWdlclJhbmdlPSh0eXBlTmFtZSx2YWx1ZSxtaW5SYW5nZSxtYXhSYW5nZSk9PntpZih2YWx1ZTxtaW5SYW5nZXx8dmFsdWU+bWF4UmFuZ2Upe3Rocm93IG5ldyBUeXBlRXJyb3IoYFBhc3NpbmcgYSBudW1iZXIgIiR7ZW1iaW5kUmVwcih2YWx1ZSl9IiBmcm9tIEpTIHNpZGUgdG8gQy9DKysgc2lkZSB0byBhbiBhcmd1bWVudCBvZiB0eXBlICIke3R5cGVOYW1lfSIsIHdoaWNoIGlzIG91dHNpZGUgdGhlIHZhbGlkIHJhbmdlIFske21pblJhbmdlfSwgJHttYXhSYW5nZX1dIWApfX07dmFyIF9fZW1iaW5kX3JlZ2lzdGVyX2JpZ2ludD0ocHJpbWl0aXZlVHlwZSxuYW1lLHNpemUsbWluUmFuZ2UsbWF4UmFuZ2UpPT57bmFtZT1Bc2NpaVRvU3RyaW5nKG5hbWUpO2NvbnN0IGlzVW5zaWduZWRUeXBlPW1pblJhbmdlPT09MG47bGV0IGZyb21XaXJlVHlwZT12YWx1ZT0+dmFsdWU7aWYoaXNVbnNpZ25lZFR5cGUpe2NvbnN0IGJpdFNpemU9c2l6ZSo4O2Zyb21XaXJlVHlwZT12YWx1ZT0+QmlnSW50LmFzVWludE4oYml0U2l6ZSx2YWx1ZSk7bWF4UmFuZ2U9ZnJvbVdpcmVUeXBlKG1heFJhbmdlKX1yZWdpc3RlclR5cGUocHJpbWl0aXZlVHlwZSx7bmFtZSxmcm9tV2lyZVR5cGUsdG9XaXJlVHlwZTooZGVzdHJ1Y3RvcnMsdmFsdWUpPT57aWYodHlwZW9mIHZhbHVlPT0ibnVtYmVyIil7dmFsdWU9QmlnSW50KHZhbHVlKX1lbHNlIGlmKHR5cGVvZiB2YWx1ZSE9ImJpZ2ludCIpe3Rocm93IG5ldyBUeXBlRXJyb3IoYENhbm5vdCBjb252ZXJ0ICIke2VtYmluZFJlcHIodmFsdWUpfSIgdG8gJHt0aGlzLm5hbWV9YCl9YXNzZXJ0SW50ZWdlclJhbmdlKG5hbWUsdmFsdWUsbWluUmFuZ2UsbWF4UmFuZ2UpO3JldHVybiB2YWx1ZX0scmVhZFZhbHVlRnJvbVBvaW50ZXI6aW50ZWdlclJlYWRWYWx1ZUZyb21Qb2ludGVyKG5hbWUsc2l6ZSwhaXNVbnNpZ25lZFR5cGUpLGRlc3RydWN0b3JGdW5jdGlvbjpudWxsfSl9O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9ib29sPShyYXdUeXBlLG5hbWUsdHJ1ZVZhbHVlLGZhbHNlVmFsdWUpPT57bmFtZT1Bc2NpaVRvU3RyaW5nKG5hbWUpO3JlZ2lzdGVyVHlwZShyYXdUeXBlLHtuYW1lLGZyb21XaXJlVHlwZTpmdW5jdGlvbih3dCl7cmV0dXJuISF3dH0sdG9XaXJlVHlwZTpmdW5jdGlvbihkZXN0cnVjdG9ycyxvKXtyZXR1cm4gbz90cnVlVmFsdWU6ZmFsc2VWYWx1ZX0scmVhZFZhbHVlRnJvbVBvaW50ZXI6ZnVuY3Rpb24ocG9pbnRlcil7cmV0dXJuIHRoaXMuZnJvbVdpcmVUeXBlKChncm93TWVtVmlld3MoKSxIRUFQVTgpW3BvaW50ZXJdKX0sZGVzdHJ1Y3RvckZ1bmN0aW9uOm51bGx9KX07dmFyIGVtdmFsX2ZyZWVsaXN0PVtdO3ZhciBlbXZhbF9oYW5kbGVzPVswLDEsLDEsbnVsbCwxLHRydWUsMSxmYWxzZSwxXTt2YXIgX19lbXZhbF9kZWNyZWY9aGFuZGxlPT57aWYoaGFuZGxlPjkmJjA9PT0tLWVtdmFsX2hhbmRsZXNbaGFuZGxlKzFdKXthc3NlcnQoZW12YWxfaGFuZGxlc1toYW5kbGVdIT09dW5kZWZpbmVkLGBEZWNyZWYgZm9yIHVuYWxsb2NhdGVkIGhhbmRsZS5gKTtlbXZhbF9oYW5kbGVzW2hhbmRsZV09dW5kZWZpbmVkO2VtdmFsX2ZyZWVsaXN0LnB1c2goaGFuZGxlKX19O3ZhciBFbXZhbD17dG9WYWx1ZTpoYW5kbGU9PntpZighaGFuZGxlKXt0aHJvd0JpbmRpbmdFcnJvcihgQ2Fubm90IHVzZSBkZWxldGVkIHZhbC4gaGFuZGxlID0gJHtoYW5kbGV9YCl9YXNzZXJ0KGhhbmRsZT09PTJ8fGVtdmFsX2hhbmRsZXNbaGFuZGxlXSE9PXVuZGVmaW5lZCYmaGFuZGxlJTI9PT0wLGBpbnZhbGlkIGhhbmRsZTogJHtoYW5kbGV9YCk7cmV0dXJuIGVtdmFsX2hhbmRsZXNbaGFuZGxlXX0sdG9IYW5kbGU6dmFsdWU9Pntzd2l0Y2godmFsdWUpe2Nhc2UgdW5kZWZpbmVkOnJldHVybiAyO2Nhc2UgbnVsbDpyZXR1cm4gNDtjYXNlIHRydWU6cmV0dXJuIDY7Y2FzZSBmYWxzZTpyZXR1cm4gODtkZWZhdWx0Ontjb25zdCBoYW5kbGU9ZW12YWxfZnJlZWxpc3QucG9wKCl8fGVtdmFsX2hhbmRsZXMubGVuZ3RoO2VtdmFsX2hhbmRsZXNbaGFuZGxlXT12YWx1ZTtlbXZhbF9oYW5kbGVzW2hhbmRsZSsxXT0xO3JldHVybiBoYW5kbGV9fX19O2Z1bmN0aW9uIHJlYWRQb2ludGVyKHBvaW50ZXIpe3JldHVybiB0aGlzLmZyb21XaXJlVHlwZSgoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcG9pbnRlcj4+Ml0pfXZhciBFbVZhbFR5cGU9e25hbWU6ImVtc2NyaXB0ZW46OnZhbCIsZnJvbVdpcmVUeXBlOmhhbmRsZT0+e3ZhciBydj1FbXZhbC50b1ZhbHVlKGhhbmRsZSk7X19lbXZhbF9kZWNyZWYoaGFuZGxlKTtyZXR1cm4gcnZ9LHRvV2lyZVR5cGU6KGRlc3RydWN0b3JzLHZhbHVlKT0+RW12YWwudG9IYW5kbGUodmFsdWUpLHJlYWRWYWx1ZUZyb21Qb2ludGVyOnJlYWRQb2ludGVyLGRlc3RydWN0b3JGdW5jdGlvbjpudWxsfTt2YXIgX19lbWJpbmRfcmVnaXN0ZXJfZW12YWw9cmF3VHlwZT0+cmVnaXN0ZXJUeXBlKHJhd1R5cGUsRW1WYWxUeXBlKTt2YXIgZmxvYXRSZWFkVmFsdWVGcm9tUG9pbnRlcj0obmFtZSx3aWR0aCk9Pntzd2l0Y2god2lkdGgpe2Nhc2UgNDpyZXR1cm4gZnVuY3Rpb24ocG9pbnRlcil7cmV0dXJuIHRoaXMuZnJvbVdpcmVUeXBlKChncm93TWVtVmlld3MoKSxIRUFQRjMyKVtwb2ludGVyPj4yXSl9O2Nhc2UgODpyZXR1cm4gZnVuY3Rpb24ocG9pbnRlcil7cmV0dXJuIHRoaXMuZnJvbVdpcmVUeXBlKChncm93TWVtVmlld3MoKSxIRUFQRjY0KVtwb2ludGVyPj4zXSl9O2RlZmF1bHQ6dGhyb3cgbmV3IFR5cGVFcnJvcihgaW52YWxpZCBmbG9hdCB3aWR0aCAoJHt3aWR0aH0pOiAke25hbWV9YCl9fTt2YXIgX19lbWJpbmRfcmVnaXN0ZXJfZmxvYXQ9KHJhd1R5cGUsbmFtZSxzaXplKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtyZWdpc3RlclR5cGUocmF3VHlwZSx7bmFtZSxmcm9tV2lyZVR5cGU6dmFsdWU9PnZhbHVlLHRvV2lyZVR5cGU6KGRlc3RydWN0b3JzLHZhbHVlKT0+e2lmKHR5cGVvZiB2YWx1ZSE9Im51bWJlciImJnR5cGVvZiB2YWx1ZSE9ImJvb2xlYW4iKXt0aHJvdyBuZXcgVHlwZUVycm9yKGBDYW5ub3QgY29udmVydCAke2VtYmluZFJlcHIodmFsdWUpfSB0byAke3RoaXMubmFtZX1gKX1yZXR1cm4gdmFsdWV9LHJlYWRWYWx1ZUZyb21Qb2ludGVyOmZsb2F0UmVhZFZhbHVlRnJvbVBvaW50ZXIobmFtZSxzaXplKSxkZXN0cnVjdG9yRnVuY3Rpb246bnVsbH0pfTt2YXIgY3JlYXRlTmFtZWRGdW5jdGlvbj0obmFtZSxmdW5jKT0+T2JqZWN0LmRlZmluZVByb3BlcnR5KGZ1bmMsIm5hbWUiLHt2YWx1ZTpuYW1lfSk7dmFyIHJ1bkRlc3RydWN0b3JzPWRlc3RydWN0b3JzPT57d2hpbGUoZGVzdHJ1Y3RvcnMubGVuZ3RoKXt2YXIgcHRyPWRlc3RydWN0b3JzLnBvcCgpO3ZhciBkZWw9ZGVzdHJ1Y3RvcnMucG9wKCk7ZGVsKHB0cil9fTtmdW5jdGlvbiB1c2VzRGVzdHJ1Y3RvclN0YWNrKGFyZ1R5cGVzKXtmb3IodmFyIGk9MTtpPGFyZ1R5cGVzLmxlbmd0aDsrK2kpe2lmKGFyZ1R5cGVzW2ldIT09bnVsbCYmYXJnVHlwZXNbaV0uZGVzdHJ1Y3RvckZ1bmN0aW9uPT09dW5kZWZpbmVkKXtyZXR1cm4gdHJ1ZX19cmV0dXJuIGZhbHNlfWZ1bmN0aW9uIGNoZWNrQXJnQ291bnQobnVtQXJncyxtaW5BcmdzLG1heEFyZ3MsaHVtYW5OYW1lLHRocm93QmluZGluZ0Vycm9yKXtpZihudW1BcmdzPG1pbkFyZ3N8fG51bUFyZ3M+bWF4QXJncyl7dmFyIGFyZ0NvdW50TWVzc2FnZT1taW5BcmdzPT1tYXhBcmdzP21pbkFyZ3M6YCR7bWluQXJnc30gdG8gJHttYXhBcmdzfWA7dGhyb3dCaW5kaW5nRXJyb3IoYGZ1bmN0aW9uICR7aHVtYW5OYW1lfSBjYWxsZWQgd2l0aCAke251bUFyZ3N9IGFyZ3VtZW50cywgZXhwZWN0ZWQgJHthcmdDb3VudE1lc3NhZ2V9YCl9fWZ1bmN0aW9uIGNyZWF0ZUpzSW52b2tlcihhcmdUeXBlcyxpc0NsYXNzTWV0aG9kRnVuYyxyZXR1cm5zLGlzQXN5bmMpe3ZhciBuZWVkc0Rlc3RydWN0b3JTdGFjaz11c2VzRGVzdHJ1Y3RvclN0YWNrKGFyZ1R5cGVzKTt2YXIgYXJnQ291bnQ9YXJnVHlwZXMubGVuZ3RoLTI7dmFyIGFyZ3NMaXN0PVtdO3ZhciBhcmdzTGlzdFdpcmVkPVsiZm4iXTtpZihpc0NsYXNzTWV0aG9kRnVuYyl7YXJnc0xpc3RXaXJlZC5wdXNoKCJ0aGlzV2lyZWQiKX1mb3IodmFyIGk9MDtpPGFyZ0NvdW50OysraSl7YXJnc0xpc3QucHVzaChgYXJnJHtpfWApO2FyZ3NMaXN0V2lyZWQucHVzaChgYXJnJHtpfVdpcmVkYCl9YXJnc0xpc3Q9YXJnc0xpc3Quam9pbigiLCIpO2FyZ3NMaXN0V2lyZWQ9YXJnc0xpc3RXaXJlZC5qb2luKCIsIik7dmFyIGludm9rZXJGbkJvZHk9YHJldHVybiBmdW5jdGlvbiAoJHthcmdzTGlzdH0pIHtcbmA7aW52b2tlckZuQm9keSs9ImNoZWNrQXJnQ291bnQoYXJndW1lbnRzLmxlbmd0aCwgbWluQXJncywgbWF4QXJncywgaHVtYW5OYW1lLCB0aHJvd0JpbmRpbmdFcnJvcik7XG4iO2lmKG5lZWRzRGVzdHJ1Y3RvclN0YWNrKXtpbnZva2VyRm5Cb2R5Kz0idmFyIGRlc3RydWN0b3JzID0gW107XG4ifXZhciBkdG9yU3RhY2s9bmVlZHNEZXN0cnVjdG9yU3RhY2s/ImRlc3RydWN0b3JzIjoibnVsbCI7dmFyIGFyZ3MxPVsiaHVtYW5OYW1lIiwidGhyb3dCaW5kaW5nRXJyb3IiLCJpbnZva2VyIiwiZm4iLCJydW5EZXN0cnVjdG9ycyIsImZyb21SZXRXaXJlIiwidG9DbGFzc1BhcmFtV2lyZSJdO2lmKGlzQ2xhc3NNZXRob2RGdW5jKXtpbnZva2VyRm5Cb2R5Kz1gdmFyIHRoaXNXaXJlZCA9IHRvQ2xhc3NQYXJhbVdpcmUoJHtkdG9yU3RhY2t9LCB0aGlzKTtcbmB9Zm9yKHZhciBpPTA7aTxhcmdDb3VudDsrK2kpe3ZhciBhcmdOYW1lPWB0b0FyZyR7aX1XaXJlYDtpbnZva2VyRm5Cb2R5Kz1gdmFyIGFyZyR7aX1XaXJlZCA9ICR7YXJnTmFtZX0oJHtkdG9yU3RhY2t9LCBhcmcke2l9KTtcbmA7YXJnczEucHVzaChhcmdOYW1lKX1pbnZva2VyRm5Cb2R5Kz0ocmV0dXJuc3x8aXNBc3luYz8idmFyIHJ2ID0gIjoiIikrYGludm9rZXIoJHthcmdzTGlzdFdpcmVkfSk7XG5gO3ZhciByZXR1cm5WYWw9cmV0dXJucz8icnYiOiIiO2FyZ3MxLnB1c2goIkFzeW5jaWZ5Iik7aW52b2tlckZuQm9keSs9YGZ1bmN0aW9uIG9uRG9uZSgke3JldHVyblZhbH0pIHtcbmA7aWYobmVlZHNEZXN0cnVjdG9yU3RhY2spe2ludm9rZXJGbkJvZHkrPSJydW5EZXN0cnVjdG9ycyhkZXN0cnVjdG9ycyk7XG4ifWVsc2V7Zm9yKHZhciBpPWlzQ2xhc3NNZXRob2RGdW5jPzE6MjtpPGFyZ1R5cGVzLmxlbmd0aDsrK2kpe3ZhciBwYXJhbU5hbWU9aT09PTE/InRoaXNXaXJlZCI6ImFyZyIrKGktMikrIldpcmVkIjtpZihhcmdUeXBlc1tpXS5kZXN0cnVjdG9yRnVuY3Rpb24hPT1udWxsKXtpbnZva2VyRm5Cb2R5Kz1gJHtwYXJhbU5hbWV9X2R0b3IoJHtwYXJhbU5hbWV9KTtcbmA7YXJnczEucHVzaChgJHtwYXJhbU5hbWV9X2R0b3JgKX19fWlmKHJldHVybnMpe2ludm9rZXJGbkJvZHkrPSJ2YXIgcmV0ID0gZnJvbVJldFdpcmUocnYpO1xuIisicmV0dXJuIHJldDtcbiJ9ZWxzZXt9aW52b2tlckZuQm9keSs9In1cbiI7aW52b2tlckZuQm9keSs9YHJldHVybiBBc3luY2lmeS5jdXJyRGF0YSA/IEFzeW5jaWZ5LndoZW5Eb25lKCkudGhlbihvbkRvbmUpIDogb25Eb25lKCR7cmV0dXJuVmFsfSk7XG5gO2ludm9rZXJGbkJvZHkrPSJ9XG4iO2FyZ3MxLnB1c2goImNoZWNrQXJnQ291bnQiLCJtaW5BcmdzIiwibWF4QXJncyIpO2ludm9rZXJGbkJvZHk9YGlmIChhcmd1bWVudHMubGVuZ3RoICE9PSAke2FyZ3MxLmxlbmd0aH0peyB0aHJvdyBuZXcgRXJyb3IoaHVtYW5OYW1lICsgIkV4cGVjdGVkICR7YXJnczEubGVuZ3RofSBjbG9zdXJlIGFyZ3VtZW50cyAiICsgYXJndW1lbnRzLmxlbmd0aCArICIgZ2l2ZW4uIik7IH1cbiR7aW52b2tlckZuQm9keX1gO3JldHVybiBuZXcgRnVuY3Rpb24oYXJnczEsaW52b2tlckZuQm9keSl9dmFyIHJ1bkFuZEFib3J0SWZFcnJvcj1mdW5jPT57dHJ5e3JldHVybiBmdW5jKCl9Y2F0Y2goZSl7YWJvcnQoZSl9fTt2YXIgaGFuZGxlRXhjZXB0aW9uPWU9PntpZihlIGluc3RhbmNlb2YgRXhpdFN0YXR1c3x8ZT09InVud2luZCIpe3JldHVybiBFWElUU1RBVFVTfWNoZWNrU3RhY2tDb29raWUoKTtpZihlIGluc3RhbmNlb2YgV2ViQXNzZW1ibHkuUnVudGltZUVycm9yKXtpZihfZW1zY3JpcHRlbl9zdGFja19nZXRfY3VycmVudCgpPD0wKXtlcnIoIlN0YWNrIG92ZXJmbG93IGRldGVjdGVkLiAgWW91IGNhbiB0cnkgaW5jcmVhc2luZyAtc1NUQUNLX1NJWkUgKGN1cnJlbnRseSBzZXQgdG8gNjU1MzYpIil9fXF1aXRfKDEsZSl9O3ZhciBtYXliZUV4aXQ9KCk9PntpZigha2VlcFJ1bnRpbWVBbGl2ZSgpKXt0cnl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCl7aWYoX3B0aHJlYWRfc2VsZigpKV9fZW1zY3JpcHRlbl90aHJlYWRfZXhpdChFWElUU1RBVFVTKTtyZXR1cm59X2V4aXQoRVhJVFNUQVRVUyl9Y2F0Y2goZSl7aGFuZGxlRXhjZXB0aW9uKGUpfX19O3ZhciBjYWxsVXNlckNhbGxiYWNrPWZ1bmM9PntpZihBQk9SVCl7ZXJyKCJ1c2VyIGNhbGxiYWNrIHRyaWdnZXJlZCBhZnRlciBydW50aW1lIGV4aXRlZCBvciBhcHBsaWNhdGlvbiBhYm9ydGVkLiAgSWdub3JpbmcuIik7cmV0dXJufXRyeXtmdW5jKCk7bWF5YmVFeGl0KCl9Y2F0Y2goZSl7aGFuZGxlRXhjZXB0aW9uKGUpfX07dmFyIHJ1bnRpbWVLZWVwYWxpdmVQdXNoPSgpPT57cnVudGltZUtlZXBhbGl2ZUNvdW50ZXIrPTF9O3ZhciBydW50aW1lS2VlcGFsaXZlUG9wPSgpPT57YXNzZXJ0KHJ1bnRpbWVLZWVwYWxpdmVDb3VudGVyPjApO3J1bnRpbWVLZWVwYWxpdmVDb3VudGVyLT0xfTt2YXIgQXN5bmNpZnk9e2luc3RydW1lbnRXYXNtSW1wb3J0cyhpbXBvcnRzKXt2YXIgaW1wb3J0UGF0dGVybj0vXihpbnZva2VfLip8X19hc3luY2pzX18uKikkLztmb3IobGV0W3gsb3JpZ2luYWxdb2YgT2JqZWN0LmVudHJpZXMoaW1wb3J0cykpe2lmKHR5cGVvZiBvcmlnaW5hbD09ImZ1bmN0aW9uIil7bGV0IGlzQXN5bmNpZnlJbXBvcnQ9b3JpZ2luYWwuaXNBc3luY3x8aW1wb3J0UGF0dGVybi50ZXN0KHgpO2ltcG9ydHNbeF09KC4uLmFyZ3MpPT57dmFyIG9yaWdpbmFsQXN5bmNpZnlTdGF0ZT1Bc3luY2lmeS5zdGF0ZTt0cnl7cmV0dXJuIG9yaWdpbmFsKC4uLmFyZ3MpfWZpbmFsbHl7dmFyIGNoYW5nZWRUb0Rpc2FibGVkPW9yaWdpbmFsQXN5bmNpZnlTdGF0ZT09PUFzeW5jaWZ5LlN0YXRlLk5vcm1hbCYmQXN5bmNpZnkuc3RhdGU9PT1Bc3luY2lmeS5TdGF0ZS5EaXNhYmxlZDt2YXIgaWdub3JlZEludm9rZT14LnN0YXJ0c1dpdGgoImludm9rZV8iKSYmdHJ1ZTtpZihBc3luY2lmeS5zdGF0ZSE9PW9yaWdpbmFsQXN5bmNpZnlTdGF0ZSYmIWlzQXN5bmNpZnlJbXBvcnQmJiFjaGFuZ2VkVG9EaXNhYmxlZCYmIWlnbm9yZWRJbnZva2Upe2Fib3J0KGBpbXBvcnQgJHt4fSB3YXMgbm90IGluIEFTWU5DSUZZX0lNUE9SVFMsIGJ1dCBjaGFuZ2VkIHRoZSBzdGF0ZWApfX19fX19LGluc3RydW1lbnRGdW5jdGlvbihvcmlnaW5hbCl7dmFyIHdyYXBwZXI9KC4uLmFyZ3MpPT57QXN5bmNpZnkuZXhwb3J0Q2FsbFN0YWNrLnB1c2gob3JpZ2luYWwpO3RyeXtyZXR1cm4gb3JpZ2luYWwoLi4uYXJncyl9ZmluYWxseXtpZighQUJPUlQpe3ZhciB0b3A9QXN5bmNpZnkuZXhwb3J0Q2FsbFN0YWNrLnBvcCgpO2Fzc2VydCh0b3A9PT1vcmlnaW5hbCk7QXN5bmNpZnkubWF5YmVTdG9wVW53aW5kKCl9fX07QXN5bmNpZnkuZnVuY1dyYXBwZXJzLnNldChvcmlnaW5hbCx3cmFwcGVyKTt3cmFwcGVyPWNyZWF0ZU5hbWVkRnVuY3Rpb24oYF9fYXN5bmNpZnlfd3JhcHBlcl8ke29yaWdpbmFsLm5hbWV9YCx3cmFwcGVyKTtyZXR1cm4gd3JhcHBlcn0saW5zdHJ1bWVudFdhc21FeHBvcnRzKGV4cG9ydHMpe3ZhciByZXQ9e307Zm9yKGxldFt4LG9yaWdpbmFsXW9mIE9iamVjdC5lbnRyaWVzKGV4cG9ydHMpKXtpZih0eXBlb2Ygb3JpZ2luYWw9PSJmdW5jdGlvbiIpe3ZhciB3cmFwcGVyPUFzeW5jaWZ5Lmluc3RydW1lbnRGdW5jdGlvbihvcmlnaW5hbCk7cmV0W3hdPXdyYXBwZXJ9ZWxzZXtyZXRbeF09b3JpZ2luYWx9fXJldHVybiByZXR9LFN0YXRlOntOb3JtYWw6MCxVbndpbmRpbmc6MSxSZXdpbmRpbmc6MixEaXNhYmxlZDozfSxzdGF0ZTowLFN0YWNrU2l6ZTo0MDk2LGN1cnJEYXRhOm51bGwsaGFuZGxlU2xlZXBSZXR1cm5WYWx1ZTowLGV4cG9ydENhbGxTdGFjazpbXSxjYWxsc3RhY2tGdW5jVG9JZDpuZXcgTWFwLGNhbGxTdGFja0lkVG9GdW5jOm5ldyBNYXAsZnVuY1dyYXBwZXJzOm5ldyBNYXAsY2FsbFN0YWNrSWQ6MCxhc3luY1Byb21pc2VIYW5kbGVyczpudWxsLHNsZWVwQ2FsbGJhY2tzOltdLGdldENhbGxTdGFja0lkKGZ1bmMpe2Fzc2VydChmdW5jKTtpZighQXN5bmNpZnkuY2FsbHN0YWNrRnVuY1RvSWQuaGFzKGZ1bmMpKXt2YXIgaWQ9QXN5bmNpZnkuY2FsbFN0YWNrSWQrKztBc3luY2lmeS5jYWxsc3RhY2tGdW5jVG9JZC5zZXQoZnVuYyxpZCk7QXN5bmNpZnkuY2FsbFN0YWNrSWRUb0Z1bmMuc2V0KGlkLGZ1bmMpfXJldHVybiBBc3luY2lmeS5jYWxsc3RhY2tGdW5jVG9JZC5nZXQoZnVuYyl9LG1heWJlU3RvcFVud2luZCgpe2lmKEFzeW5jaWZ5LmN1cnJEYXRhJiZBc3luY2lmeS5zdGF0ZT09PUFzeW5jaWZ5LlN0YXRlLlVud2luZGluZyYmQXN5bmNpZnkuZXhwb3J0Q2FsbFN0YWNrLmxlbmd0aD09PTApe0FzeW5jaWZ5LnN0YXRlPUFzeW5jaWZ5LlN0YXRlLk5vcm1hbDtydW50aW1lS2VlcGFsaXZlUHVzaCgpO3J1bkFuZEFib3J0SWZFcnJvcihfYXN5bmNpZnlfc3RvcF91bndpbmQpO2lmKHR5cGVvZiBGaWJlcnMhPSJ1bmRlZmluZWQiKXtGaWJlcnMudHJhbXBvbGluZSgpfX19LHdoZW5Eb25lKCl7YXNzZXJ0KEFzeW5jaWZ5LmN1cnJEYXRhLCJUcmllZCB0byB3YWl0IGZvciBhbiBhc3luYyBvcGVyYXRpb24gd2hlbiBub25lIGlzIGluIHByb2dyZXNzLiIpO2Fzc2VydCghQXN5bmNpZnkuYXN5bmNQcm9taXNlSGFuZGxlcnMsIkNhbm5vdCBoYXZlIG11bHRpcGxlIGFzeW5jIG9wZXJhdGlvbnMgaW4gZmxpZ2h0IGF0IG9uY2UiKTtyZXR1cm4gbmV3IFByb21pc2UoKHJlc29sdmUscmVqZWN0KT0+e0FzeW5jaWZ5LmFzeW5jUHJvbWlzZUhhbmRsZXJzPXtyZXNvbHZlLHJlamVjdH19KX0sYWxsb2NhdGVEYXRhKCl7dmFyIHB0cj1fbWFsbG9jKDEyK0FzeW5jaWZ5LlN0YWNrU2l6ZSk7QXN5bmNpZnkuc2V0RGF0YUhlYWRlcihwdHIscHRyKzEyLEFzeW5jaWZ5LlN0YWNrU2l6ZSk7QXN5bmNpZnkuc2V0RGF0YVJld2luZEZ1bmMocHRyKTtyZXR1cm4gcHRyfSxzZXREYXRhSGVhZGVyKHB0cixzdGFjayxzdGFja1NpemUpeyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwdHI+PjJdPXN0YWNrOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwdHIrND4+Ml09c3RhY2src3RhY2tTaXplfSxzZXREYXRhUmV3aW5kRnVuYyhwdHIpe3ZhciBib3R0b21PZkNhbGxTdGFjaz1Bc3luY2lmeS5leHBvcnRDYWxsU3RhY2tbMF07YXNzZXJ0KGJvdHRvbU9mQ2FsbFN0YWNrLCJleHBvcnRDYWxsU3RhY2sgaXMgZW1wdHkiKTt2YXIgcmV3aW5kSWQ9QXN5bmNpZnkuZ2V0Q2FsbFN0YWNrSWQoYm90dG9tT2ZDYWxsU3RhY2spOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW3B0cis4Pj4yXT1yZXdpbmRJZH0sZ2V0RGF0YVJld2luZEZ1bmMocHRyKXt2YXIgaWQ9KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbcHRyKzg+PjJdO3ZhciBmdW5jPUFzeW5jaWZ5LmNhbGxTdGFja0lkVG9GdW5jLmdldChpZCk7YXNzZXJ0KGZ1bmMsYGlkICR7aWR9IG5vdCBmb3VuZCBpbiBjYWxsU3RhY2tJZFRvRnVuY2ApO3JldHVybiBmdW5jfSxkb1Jld2luZChwdHIpe3ZhciBvcmlnaW5hbD1Bc3luY2lmeS5nZXREYXRhUmV3aW5kRnVuYyhwdHIpO3ZhciBmdW5jPUFzeW5jaWZ5LmZ1bmNXcmFwcGVycy5nZXQob3JpZ2luYWwpO2Fzc2VydChvcmlnaW5hbCk7YXNzZXJ0KGZ1bmMpO3J1bnRpbWVLZWVwYWxpdmVQb3AoKTtyZXR1cm4gZnVuYygpfSxoYW5kbGVTbGVlcChzdGFydEFzeW5jKXthc3NlcnQoQXN5bmNpZnkuc3RhdGUhPT1Bc3luY2lmeS5TdGF0ZS5EaXNhYmxlZCwiQXN5bmNpZnkgY2Fubm90IGJlIGRvbmUgZHVyaW5nIG9yIGFmdGVyIHRoZSBydW50aW1lIGV4aXRzIik7aWYoQUJPUlQpcmV0dXJuO2lmKEFzeW5jaWZ5LnN0YXRlPT09QXN5bmNpZnkuU3RhdGUuTm9ybWFsKXt2YXIgcmVhY2hlZENhbGxiYWNrPWZhbHNlO3ZhciByZWFjaGVkQWZ0ZXJDYWxsYmFjaz1mYWxzZTtzdGFydEFzeW5jKChoYW5kbGVTbGVlcFJldHVyblZhbHVlPTApPT57YXNzZXJ0KCFoYW5kbGVTbGVlcFJldHVyblZhbHVlfHx0eXBlb2YgaGFuZGxlU2xlZXBSZXR1cm5WYWx1ZT09Im51bWJlciJ8fHR5cGVvZiBoYW5kbGVTbGVlcFJldHVyblZhbHVlPT0iYm9vbGVhbiIpO2lmKEFCT1JUKXJldHVybjtBc3luY2lmeS5oYW5kbGVTbGVlcFJldHVyblZhbHVlPWhhbmRsZVNsZWVwUmV0dXJuVmFsdWU7cmVhY2hlZENhbGxiYWNrPXRydWU7aWYoIXJlYWNoZWRBZnRlckNhbGxiYWNrKXtyZXR1cm59YXNzZXJ0KCFBc3luY2lmeS5leHBvcnRDYWxsU3RhY2subGVuZ3RoLCJXYWtpbmcgdXAgKHN0YXJ0aW5nIHRvIHJld2luZCkgbXVzdCBiZSBkb25lIGZyb20gSlMsIHdpdGhvdXQgY29tcGlsZWQgY29kZSBvbiB0aGUgc3RhY2suIik7QXN5bmNpZnkuc3RhdGU9QXN5bmNpZnkuU3RhdGUuUmV3aW5kaW5nO3J1bkFuZEFib3J0SWZFcnJvcigoKT0+X2FzeW5jaWZ5X3N0YXJ0X3Jld2luZChBc3luY2lmeS5jdXJyRGF0YSkpO2lmKHR5cGVvZiBNYWluTG9vcCE9InVuZGVmaW5lZCImJk1haW5Mb29wLmZ1bmMpe01haW5Mb29wLnJlc3VtZSgpfXZhciBhc3luY1dhc21SZXR1cm5WYWx1ZSxpc0Vycm9yPWZhbHNlO3RyeXthc3luY1dhc21SZXR1cm5WYWx1ZT1Bc3luY2lmeS5kb1Jld2luZChBc3luY2lmeS5jdXJyRGF0YSl9Y2F0Y2goZXJyKXthc3luY1dhc21SZXR1cm5WYWx1ZT1lcnI7aXNFcnJvcj10cnVlfXZhciBoYW5kbGVkPWZhbHNlO2lmKCFBc3luY2lmeS5jdXJyRGF0YSl7dmFyIGFzeW5jUHJvbWlzZUhhbmRsZXJzPUFzeW5jaWZ5LmFzeW5jUHJvbWlzZUhhbmRsZXJzO2lmKGFzeW5jUHJvbWlzZUhhbmRsZXJzKXtBc3luY2lmeS5hc3luY1Byb21pc2VIYW5kbGVycz1udWxsOyhpc0Vycm9yP2FzeW5jUHJvbWlzZUhhbmRsZXJzLnJlamVjdDphc3luY1Byb21pc2VIYW5kbGVycy5yZXNvbHZlKShhc3luY1dhc21SZXR1cm5WYWx1ZSk7aGFuZGxlZD10cnVlfX1pZihpc0Vycm9yJiYhaGFuZGxlZCl7dGhyb3cgYXN5bmNXYXNtUmV0dXJuVmFsdWV9fSk7cmVhY2hlZEFmdGVyQ2FsbGJhY2s9dHJ1ZTtpZighcmVhY2hlZENhbGxiYWNrKXtBc3luY2lmeS5zdGF0ZT1Bc3luY2lmeS5TdGF0ZS5VbndpbmRpbmc7QXN5bmNpZnkuY3VyckRhdGE9QXN5bmNpZnkuYWxsb2NhdGVEYXRhKCk7aWYodHlwZW9mIE1haW5Mb29wIT0idW5kZWZpbmVkIiYmTWFpbkxvb3AuZnVuYyl7TWFpbkxvb3AucGF1c2UoKX1ydW5BbmRBYm9ydElmRXJyb3IoKCk9Pl9hc3luY2lmeV9zdGFydF91bndpbmQoQXN5bmNpZnkuY3VyckRhdGEpKX19ZWxzZSBpZihBc3luY2lmeS5zdGF0ZT09PUFzeW5jaWZ5LlN0YXRlLlJld2luZGluZyl7QXN5bmNpZnkuc3RhdGU9QXN5bmNpZnkuU3RhdGUuTm9ybWFsO3J1bkFuZEFib3J0SWZFcnJvcihfYXN5bmNpZnlfc3RvcF9yZXdpbmQpO19mcmVlKEFzeW5jaWZ5LmN1cnJEYXRhKTtBc3luY2lmeS5jdXJyRGF0YT1udWxsO0FzeW5jaWZ5LnNsZWVwQ2FsbGJhY2tzLmZvckVhY2goY2FsbFVzZXJDYWxsYmFjayl9ZWxzZXthYm9ydChgaW52YWxpZCBzdGF0ZTogJHtBc3luY2lmeS5zdGF0ZX1gKX1yZXR1cm4gQXN5bmNpZnkuaGFuZGxlU2xlZXBSZXR1cm5WYWx1ZX0saGFuZGxlQXN5bmM6c3RhcnRBc3luYz0+QXN5bmNpZnkuaGFuZGxlU2xlZXAod2FrZVVwPT57c3RhcnRBc3luYygpLnRoZW4od2FrZVVwKX0pfTtmdW5jdGlvbiBnZXRSZXF1aXJlZEFyZ0NvdW50KGFyZ1R5cGVzKXt2YXIgcmVxdWlyZWRBcmdDb3VudD1hcmdUeXBlcy5sZW5ndGgtMjtmb3IodmFyIGk9YXJnVHlwZXMubGVuZ3RoLTE7aT49MjstLWkpe2lmKCFhcmdUeXBlc1tpXS5vcHRpb25hbCl7YnJlYWt9cmVxdWlyZWRBcmdDb3VudC0tfXJldHVybiByZXF1aXJlZEFyZ0NvdW50fWZ1bmN0aW9uIGNyYWZ0SW52b2tlckZ1bmN0aW9uKGh1bWFuTmFtZSxhcmdUeXBlcyxjbGFzc1R5cGUsY3BwSW52b2tlckZ1bmMsY3BwVGFyZ2V0RnVuYyxpc0FzeW5jKXt2YXIgYXJnQ291bnQ9YXJnVHlwZXMubGVuZ3RoO2lmKGFyZ0NvdW50PDIpe3Rocm93QmluZGluZ0Vycm9yKCJhcmdUeXBlcyBhcnJheSBzaXplIG1pc21hdGNoISBNdXN0IGF0IGxlYXN0IGdldCByZXR1cm4gdmFsdWUgYW5kICd0aGlzJyB0eXBlcyEiKX1hc3NlcnQoIWlzQXN5bmMsIkFzeW5jIGJpbmRpbmdzIGFyZSBvbmx5IHN1cHBvcnRlZCB3aXRoIEpTUEkuIik7dmFyIGlzQ2xhc3NNZXRob2RGdW5jPWFyZ1R5cGVzWzFdIT09bnVsbCYmY2xhc3NUeXBlIT09bnVsbDt2YXIgbmVlZHNEZXN0cnVjdG9yU3RhY2s9dXNlc0Rlc3RydWN0b3JTdGFjayhhcmdUeXBlcyk7dmFyIHJldHVybnM9IWFyZ1R5cGVzWzBdLmlzVm9pZDt2YXIgZXhwZWN0ZWRBcmdDb3VudD1hcmdDb3VudC0yO3ZhciBtaW5BcmdzPWdldFJlcXVpcmVkQXJnQ291bnQoYXJnVHlwZXMpO3ZhciByZXRUeXBlPWFyZ1R5cGVzWzBdO3ZhciBpbnN0VHlwZT1hcmdUeXBlc1sxXTt2YXIgY2xvc3VyZUFyZ3M9W2h1bWFuTmFtZSx0aHJvd0JpbmRpbmdFcnJvcixjcHBJbnZva2VyRnVuYyxjcHBUYXJnZXRGdW5jLHJ1bkRlc3RydWN0b3JzLHJldFR5cGUuZnJvbVdpcmVUeXBlLmJpbmQocmV0VHlwZSksaW5zdFR5cGU/LnRvV2lyZVR5cGUuYmluZChpbnN0VHlwZSldO2Zvcih2YXIgaT0yO2k8YXJnQ291bnQ7KytpKXt2YXIgYXJnVHlwZT1hcmdUeXBlc1tpXTtjbG9zdXJlQXJncy5wdXNoKGFyZ1R5cGUudG9XaXJlVHlwZS5iaW5kKGFyZ1R5cGUpKX1jbG9zdXJlQXJncy5wdXNoKEFzeW5jaWZ5KTtpZighbmVlZHNEZXN0cnVjdG9yU3RhY2spe2Zvcih2YXIgaT1pc0NsYXNzTWV0aG9kRnVuYz8xOjI7aTxhcmdUeXBlcy5sZW5ndGg7KytpKXtpZihhcmdUeXBlc1tpXS5kZXN0cnVjdG9yRnVuY3Rpb24hPT1udWxsKXtjbG9zdXJlQXJncy5wdXNoKGFyZ1R5cGVzW2ldLmRlc3RydWN0b3JGdW5jdGlvbil9fX1jbG9zdXJlQXJncy5wdXNoKGNoZWNrQXJnQ291bnQsbWluQXJncyxleHBlY3RlZEFyZ0NvdW50KTtsZXQgaW52b2tlckZhY3Rvcnk9Y3JlYXRlSnNJbnZva2VyKGFyZ1R5cGVzLGlzQ2xhc3NNZXRob2RGdW5jLHJldHVybnMsaXNBc3luYyk7dmFyIGludm9rZXJGbj1pbnZva2VyRmFjdG9yeSguLi5jbG9zdXJlQXJncyk7cmV0dXJuIGNyZWF0ZU5hbWVkRnVuY3Rpb24oaHVtYW5OYW1lLGludm9rZXJGbil9dmFyIGVuc3VyZU92ZXJsb2FkVGFibGU9KHByb3RvLG1ldGhvZE5hbWUsaHVtYW5OYW1lKT0+e2lmKHVuZGVmaW5lZD09PXByb3RvW21ldGhvZE5hbWVdLm92ZXJsb2FkVGFibGUpe3ZhciBwcmV2RnVuYz1wcm90b1ttZXRob2ROYW1lXTtwcm90b1ttZXRob2ROYW1lXT1mdW5jdGlvbiguLi5hcmdzKXtpZighcHJvdG9bbWV0aG9kTmFtZV0ub3ZlcmxvYWRUYWJsZS5oYXNPd25Qcm9wZXJ0eShhcmdzLmxlbmd0aCkpe3Rocm93QmluZGluZ0Vycm9yKGBGdW5jdGlvbiAnJHtodW1hbk5hbWV9JyBjYWxsZWQgd2l0aCBhbiBpbnZhbGlkIG51bWJlciBvZiBhcmd1bWVudHMgKCR7YXJncy5sZW5ndGh9KSAtIGV4cGVjdHMgb25lIG9mICgke3Byb3RvW21ldGhvZE5hbWVdLm92ZXJsb2FkVGFibGV9KSFgKX1yZXR1cm4gcHJvdG9bbWV0aG9kTmFtZV0ub3ZlcmxvYWRUYWJsZVthcmdzLmxlbmd0aF0uYXBwbHkodGhpcyxhcmdzKX07cHJvdG9bbWV0aG9kTmFtZV0ub3ZlcmxvYWRUYWJsZT1bXTtwcm90b1ttZXRob2ROYW1lXS5vdmVybG9hZFRhYmxlW3ByZXZGdW5jLmFyZ0NvdW50XT1wcmV2RnVuY319O3ZhciBleHBvc2VQdWJsaWNTeW1ib2w9KG5hbWUsdmFsdWUsbnVtQXJndW1lbnRzKT0+e2lmKE1vZHVsZS5oYXNPd25Qcm9wZXJ0eShuYW1lKSl7aWYodW5kZWZpbmVkPT09bnVtQXJndW1lbnRzfHx1bmRlZmluZWQhPT1Nb2R1bGVbbmFtZV0ub3ZlcmxvYWRUYWJsZSYmdW5kZWZpbmVkIT09TW9kdWxlW25hbWVdLm92ZXJsb2FkVGFibGVbbnVtQXJndW1lbnRzXSl7dGhyb3dCaW5kaW5nRXJyb3IoYENhbm5vdCByZWdpc3RlciBwdWJsaWMgbmFtZSAnJHtuYW1lfScgdHdpY2VgKX1lbnN1cmVPdmVybG9hZFRhYmxlKE1vZHVsZSxuYW1lLG5hbWUpO2lmKE1vZHVsZVtuYW1lXS5vdmVybG9hZFRhYmxlLmhhc093blByb3BlcnR5KG51bUFyZ3VtZW50cykpe3Rocm93QmluZGluZ0Vycm9yKGBDYW5ub3QgcmVnaXN0ZXIgbXVsdGlwbGUgb3ZlcmxvYWRzIG9mIGEgZnVuY3Rpb24gd2l0aCB0aGUgc2FtZSBudW1iZXIgb2YgYXJndW1lbnRzICgke251bUFyZ3VtZW50c30pIWApfU1vZHVsZVtuYW1lXS5vdmVybG9hZFRhYmxlW251bUFyZ3VtZW50c109dmFsdWV9ZWxzZXtNb2R1bGVbbmFtZV09dmFsdWU7TW9kdWxlW25hbWVdLmFyZ0NvdW50PW51bUFyZ3VtZW50c319O3ZhciBoZWFwMzJWZWN0b3JUb0FycmF5PShjb3VudCxmaXJzdEVsZW1lbnQpPT57dmFyIGFycmF5PVtdO2Zvcih2YXIgaT0wO2k8Y291bnQ7aSsrKXthcnJheS5wdXNoKChncm93TWVtVmlld3MoKSxIRUFQVTMyKVtmaXJzdEVsZW1lbnQraSo0Pj4yXSl9cmV0dXJuIGFycmF5fTt2YXIgSW50ZXJuYWxFcnJvcj1jbGFzcyBJbnRlcm5hbEVycm9yIGV4dGVuZHMgRXJyb3J7Y29uc3RydWN0b3IobWVzc2FnZSl7c3VwZXIobWVzc2FnZSk7dGhpcy5uYW1lPSJJbnRlcm5hbEVycm9yIn19O3ZhciB0aHJvd0ludGVybmFsRXJyb3I9bWVzc2FnZT0+e3Rocm93IG5ldyBJbnRlcm5hbEVycm9yKG1lc3NhZ2UpfTt2YXIgcmVwbGFjZVB1YmxpY1N5bWJvbD0obmFtZSx2YWx1ZSxudW1Bcmd1bWVudHMpPT57aWYoIU1vZHVsZS5oYXNPd25Qcm9wZXJ0eShuYW1lKSl7dGhyb3dJbnRlcm5hbEVycm9yKCJSZXBsYWNpbmcgbm9uZXhpc3RlbnQgcHVibGljIHN5bWJvbCIpfWlmKHVuZGVmaW5lZCE9PU1vZHVsZVtuYW1lXS5vdmVybG9hZFRhYmxlJiZ1bmRlZmluZWQhPT1udW1Bcmd1bWVudHMpe01vZHVsZVtuYW1lXS5vdmVybG9hZFRhYmxlW251bUFyZ3VtZW50c109dmFsdWV9ZWxzZXtNb2R1bGVbbmFtZV09dmFsdWU7TW9kdWxlW25hbWVdLmFyZ0NvdW50PW51bUFyZ3VtZW50c319O3ZhciBnZXREeW5DYWxsZXI9KHNpZyxwdHIscHJvbWlzaW5nPWZhbHNlKT0+KC4uLmFyZ3MpPT5keW5DYWxsKHNpZyxwdHIsYXJncyxwcm9taXNpbmcpO3ZhciBlbWJpbmRfX3JlcXVpcmVGdW5jdGlvbj0oc2lnbmF0dXJlLHJhd0Z1bmN0aW9uLGlzQXN5bmM9ZmFsc2UpPT57YXNzZXJ0KCFpc0FzeW5jLCJBc3luYyBiaW5kaW5ncyBhcmUgb25seSBzdXBwb3J0ZWQgd2l0aCBKU1BJLiIpO3NpZ25hdHVyZT1Bc2NpaVRvU3RyaW5nKHNpZ25hdHVyZSk7ZnVuY3Rpb24gbWFrZUR5bkNhbGxlcigpe3JldHVybiBnZXREeW5DYWxsZXIoc2lnbmF0dXJlLHJhd0Z1bmN0aW9uKX12YXIgZnA9bWFrZUR5bkNhbGxlcigpO2lmKHR5cGVvZiBmcCE9ImZ1bmN0aW9uIil7dGhyb3dCaW5kaW5nRXJyb3IoYHVua25vd24gZnVuY3Rpb24gcG9pbnRlciB3aXRoIHNpZ25hdHVyZSAke3NpZ25hdHVyZX06ICR7cmF3RnVuY3Rpb259YCl9cmV0dXJuIGZwfTtjbGFzcyBVbmJvdW5kVHlwZUVycm9yIGV4dGVuZHMgRXJyb3J7fXZhciBnZXRUeXBlTmFtZT10eXBlPT57dmFyIHB0cj1fX19nZXRUeXBlTmFtZSh0eXBlKTt2YXIgcnY9QXNjaWlUb1N0cmluZyhwdHIpO19mcmVlKHB0cik7cmV0dXJuIHJ2fTt2YXIgdGhyb3dVbmJvdW5kVHlwZUVycm9yPShtZXNzYWdlLHR5cGVzKT0+e3ZhciB1bmJvdW5kVHlwZXM9W107dmFyIHNlZW49e307ZnVuY3Rpb24gdmlzaXQodHlwZSl7aWYoc2Vlblt0eXBlXSl7cmV0dXJufWlmKHJlZ2lzdGVyZWRUeXBlc1t0eXBlXSl7cmV0dXJufWlmKHR5cGVEZXBlbmRlbmNpZXNbdHlwZV0pe3R5cGVEZXBlbmRlbmNpZXNbdHlwZV0uZm9yRWFjaCh2aXNpdCk7cmV0dXJufXVuYm91bmRUeXBlcy5wdXNoKHR5cGUpO3NlZW5bdHlwZV09dHJ1ZX10eXBlcy5mb3JFYWNoKHZpc2l0KTt0aHJvdyBuZXcgVW5ib3VuZFR5cGVFcnJvcihgJHttZXNzYWdlfTogYCt1bmJvdW5kVHlwZXMubWFwKGdldFR5cGVOYW1lKS5qb2luKFsiLCAiXSkpfTt2YXIgd2hlbkRlcGVuZGVudFR5cGVzQXJlUmVzb2x2ZWQ9KG15VHlwZXMsZGVwZW5kZW50VHlwZXMsZ2V0VHlwZUNvbnZlcnRlcnMpPT57bXlUeXBlcy5mb3JFYWNoKHR5cGU9PnR5cGVEZXBlbmRlbmNpZXNbdHlwZV09ZGVwZW5kZW50VHlwZXMpO2Z1bmN0aW9uIG9uQ29tcGxldGUodHlwZUNvbnZlcnRlcnMpe3ZhciBteVR5cGVDb252ZXJ0ZXJzPWdldFR5cGVDb252ZXJ0ZXJzKHR5cGVDb252ZXJ0ZXJzKTtpZihteVR5cGVDb252ZXJ0ZXJzLmxlbmd0aCE9PW15VHlwZXMubGVuZ3RoKXt0aHJvd0ludGVybmFsRXJyb3IoIk1pc21hdGNoZWQgdHlwZSBjb252ZXJ0ZXIgY291bnQiKX1mb3IodmFyIGk9MDtpPG15VHlwZXMubGVuZ3RoOysraSl7cmVnaXN0ZXJUeXBlKG15VHlwZXNbaV0sbXlUeXBlQ29udmVydGVyc1tpXSl9fXZhciB0eXBlQ29udmVydGVycz1uZXcgQXJyYXkoZGVwZW5kZW50VHlwZXMubGVuZ3RoKTt2YXIgdW5yZWdpc3RlcmVkVHlwZXM9W107dmFyIHJlZ2lzdGVyZWQ9MDtmb3IobGV0W2ksZHRdb2YgZGVwZW5kZW50VHlwZXMuZW50cmllcygpKXtpZihyZWdpc3RlcmVkVHlwZXMuaGFzT3duUHJvcGVydHkoZHQpKXt0eXBlQ29udmVydGVyc1tpXT1yZWdpc3RlcmVkVHlwZXNbZHRdfWVsc2V7dW5yZWdpc3RlcmVkVHlwZXMucHVzaChkdCk7aWYoIWF3YWl0aW5nRGVwZW5kZW5jaWVzLmhhc093blByb3BlcnR5KGR0KSl7YXdhaXRpbmdEZXBlbmRlbmNpZXNbZHRdPVtdfWF3YWl0aW5nRGVwZW5kZW5jaWVzW2R0XS5wdXNoKCgpPT57dHlwZUNvbnZlcnRlcnNbaV09cmVnaXN0ZXJlZFR5cGVzW2R0XTsrK3JlZ2lzdGVyZWQ7aWYocmVnaXN0ZXJlZD09PXVucmVnaXN0ZXJlZFR5cGVzLmxlbmd0aCl7b25Db21wbGV0ZSh0eXBlQ29udmVydGVycyl9fSl9fWlmKDA9PT11bnJlZ2lzdGVyZWRUeXBlcy5sZW5ndGgpe29uQ29tcGxldGUodHlwZUNvbnZlcnRlcnMpfX07dmFyIGdldEZ1bmN0aW9uTmFtZT1zaWduYXR1cmU9PntzaWduYXR1cmU9c2lnbmF0dXJlLnRyaW0oKTtjb25zdCBhcmdzSW5kZXg9c2lnbmF0dXJlLmluZGV4T2YoIigiKTtpZihhcmdzSW5kZXg9PT0tMSlyZXR1cm4gc2lnbmF0dXJlO2Fzc2VydChzaWduYXR1cmUuZW5kc1dpdGgoIikiKSwiUGFyZW50aGVzZXMgZm9yIGFyZ3VtZW50IG5hbWVzIHNob3VsZCBtYXRjaC4iKTtyZXR1cm4gc2lnbmF0dXJlLnNsaWNlKDAsYXJnc0luZGV4KX07dmFyIF9fZW1iaW5kX3JlZ2lzdGVyX2Z1bmN0aW9uPShuYW1lLGFyZ0NvdW50LHJhd0FyZ1R5cGVzQWRkcixzaWduYXR1cmUscmF3SW52b2tlcixmbixpc0FzeW5jLGlzTm9ubnVsbFJldHVybik9Pnt2YXIgYXJnVHlwZXM9aGVhcDMyVmVjdG9yVG9BcnJheShhcmdDb3VudCxyYXdBcmdUeXBlc0FkZHIpO25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtuYW1lPWdldEZ1bmN0aW9uTmFtZShuYW1lKTtyYXdJbnZva2VyPWVtYmluZF9fcmVxdWlyZUZ1bmN0aW9uKHNpZ25hdHVyZSxyYXdJbnZva2VyLGlzQXN5bmMpO2V4cG9zZVB1YmxpY1N5bWJvbChuYW1lLGZ1bmN0aW9uKCl7dGhyb3dVbmJvdW5kVHlwZUVycm9yKGBDYW5ub3QgY2FsbCAke25hbWV9IGR1ZSB0byB1bmJvdW5kIHR5cGVzYCxhcmdUeXBlcyl9LGFyZ0NvdW50LTEpO3doZW5EZXBlbmRlbnRUeXBlc0FyZVJlc29sdmVkKFtdLGFyZ1R5cGVzLGFyZ1R5cGVzPT57dmFyIGludm9rZXJBcmdzQXJyYXk9W2FyZ1R5cGVzWzBdLG51bGxdLmNvbmNhdChhcmdUeXBlcy5zbGljZSgxKSk7cmVwbGFjZVB1YmxpY1N5bWJvbChuYW1lLGNyYWZ0SW52b2tlckZ1bmN0aW9uKG5hbWUsaW52b2tlckFyZ3NBcnJheSxudWxsLHJhd0ludm9rZXIsZm4saXNBc3luYyksYXJnQ291bnQtMSk7cmV0dXJuW119KX07dmFyIF9fZW1iaW5kX3JlZ2lzdGVyX2ludGVnZXI9KHByaW1pdGl2ZVR5cGUsbmFtZSxzaXplLG1pblJhbmdlLG1heFJhbmdlKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtjb25zdCBpc1Vuc2lnbmVkVHlwZT1taW5SYW5nZT09PTA7bGV0IGZyb21XaXJlVHlwZT12YWx1ZT0+dmFsdWU7aWYoaXNVbnNpZ25lZFR5cGUpe3ZhciBiaXRzaGlmdD0zMi04KnNpemU7ZnJvbVdpcmVUeXBlPXZhbHVlPT52YWx1ZTw8Yml0c2hpZnQ+Pj5iaXRzaGlmdDttYXhSYW5nZT1mcm9tV2lyZVR5cGUobWF4UmFuZ2UpfXJlZ2lzdGVyVHlwZShwcmltaXRpdmVUeXBlLHtuYW1lLGZyb21XaXJlVHlwZSx0b1dpcmVUeXBlOihkZXN0cnVjdG9ycyx2YWx1ZSk9PntpZih0eXBlb2YgdmFsdWUhPSJudW1iZXIiJiZ0eXBlb2YgdmFsdWUhPSJib29sZWFuIil7dGhyb3cgbmV3IFR5cGVFcnJvcihgQ2Fubm90IGNvbnZlcnQgIiR7ZW1iaW5kUmVwcih2YWx1ZSl9IiB0byAke25hbWV9YCl9YXNzZXJ0SW50ZWdlclJhbmdlKG5hbWUsdmFsdWUsbWluUmFuZ2UsbWF4UmFuZ2UpO3JldHVybiB2YWx1ZX0scmVhZFZhbHVlRnJvbVBvaW50ZXI6aW50ZWdlclJlYWRWYWx1ZUZyb21Qb2ludGVyKG5hbWUsc2l6ZSxtaW5SYW5nZSE9PTApLGRlc3RydWN0b3JGdW5jdGlvbjpudWxsfSl9O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9tZW1vcnlfdmlldz0ocmF3VHlwZSxkYXRhVHlwZUluZGV4LG5hbWUpPT57dmFyIHR5cGVNYXBwaW5nPVtJbnQ4QXJyYXksVWludDhBcnJheSxJbnQxNkFycmF5LFVpbnQxNkFycmF5LEludDMyQXJyYXksVWludDMyQXJyYXksRmxvYXQzMkFycmF5LEZsb2F0NjRBcnJheSxCaWdJbnQ2NEFycmF5LEJpZ1VpbnQ2NEFycmF5XTt2YXIgVEE9dHlwZU1hcHBpbmdbZGF0YVR5cGVJbmRleF07ZnVuY3Rpb24gZGVjb2RlTWVtb3J5VmlldyhoYW5kbGUpe3ZhciBzaXplPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtoYW5kbGU+PjJdO3ZhciBkYXRhPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtoYW5kbGUrND4+Ml07cmV0dXJuIG5ldyBUQSgoZ3Jvd01lbVZpZXdzKCksSEVBUDgpLmJ1ZmZlcixkYXRhLHNpemUpfW5hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtyZWdpc3RlclR5cGUocmF3VHlwZSx7bmFtZSxmcm9tV2lyZVR5cGU6ZGVjb2RlTWVtb3J5VmlldyxyZWFkVmFsdWVGcm9tUG9pbnRlcjpkZWNvZGVNZW1vcnlWaWV3fSx7aWdub3JlRHVwbGljYXRlUmVnaXN0cmF0aW9uczp0cnVlfSl9O3ZhciBzdHJpbmdUb1VURjg9KHN0cixvdXRQdHIsbWF4Qnl0ZXNUb1dyaXRlKT0+e2Fzc2VydCh0eXBlb2YgbWF4Qnl0ZXNUb1dyaXRlPT0ibnVtYmVyIiwic3RyaW5nVG9VVEY4KHN0ciwgb3V0UHRyLCBtYXhCeXRlc1RvV3JpdGUpIGlzIG1pc3NpbmcgdGhlIHRoaXJkIHBhcmFtZXRlciB0aGF0IHNwZWNpZmllcyB0aGUgbGVuZ3RoIG9mIHRoZSBvdXRwdXQgYnVmZmVyISIpO3JldHVybiBzdHJpbmdUb1VURjhBcnJheShzdHIsKGdyb3dNZW1WaWV3cygpLEhFQVBVOCksb3V0UHRyLG1heEJ5dGVzVG9Xcml0ZSl9O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9zdGRfc3RyaW5nPShyYXdUeXBlLG5hbWUpPT57bmFtZT1Bc2NpaVRvU3RyaW5nKG5hbWUpO3ZhciBzdGRTdHJpbmdJc1VURjg9dHJ1ZTtyZWdpc3RlclR5cGUocmF3VHlwZSx7bmFtZSxmcm9tV2lyZVR5cGUodmFsdWUpe3ZhciBsZW5ndGg9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3ZhbHVlPj4yXTt2YXIgcGF5bG9hZD12YWx1ZSs0O3ZhciBzdHI7aWYoc3RkU3RyaW5nSXNVVEY4KXtzdHI9VVRGOFRvU3RyaW5nKHBheWxvYWQsbGVuZ3RoLHRydWUpfWVsc2V7c3RyPSIiO2Zvcih2YXIgaT0wO2k8bGVuZ3RoOysraSl7c3RyKz1TdHJpbmcuZnJvbUNoYXJDb2RlKChncm93TWVtVmlld3MoKSxIRUFQVTgpW3BheWxvYWQraV0pfX1fZnJlZSh2YWx1ZSk7cmV0dXJuIHN0cn0sdG9XaXJlVHlwZShkZXN0cnVjdG9ycyx2YWx1ZSl7aWYodmFsdWUgaW5zdGFuY2VvZiBBcnJheUJ1ZmZlcil7dmFsdWU9bmV3IFVpbnQ4QXJyYXkodmFsdWUpfXZhciBsZW5ndGg7dmFyIHZhbHVlSXNPZlR5cGVTdHJpbmc9dHlwZW9mIHZhbHVlPT0ic3RyaW5nIjtpZighKHZhbHVlSXNPZlR5cGVTdHJpbmd8fEFycmF5QnVmZmVyLmlzVmlldyh2YWx1ZSkmJnZhbHVlLkJZVEVTX1BFUl9FTEVNRU5UPT0xKSl7dGhyb3dCaW5kaW5nRXJyb3IoIkNhbm5vdCBwYXNzIG5vbi1zdHJpbmcgdG8gc3RkOjpzdHJpbmciKX1pZihzdGRTdHJpbmdJc1VURjgmJnZhbHVlSXNPZlR5cGVTdHJpbmcpe2xlbmd0aD1sZW5ndGhCeXRlc1VURjgodmFsdWUpfWVsc2V7bGVuZ3RoPXZhbHVlLmxlbmd0aH12YXIgYmFzZT1fbWFsbG9jKDQrbGVuZ3RoKzEpO3ZhciBwdHI9YmFzZSs0Oyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtiYXNlPj4yXT1sZW5ndGg7aWYodmFsdWVJc09mVHlwZVN0cmluZyl7aWYoc3RkU3RyaW5nSXNVVEY4KXtzdHJpbmdUb1VURjgodmFsdWUscHRyLGxlbmd0aCsxKX1lbHNle2Zvcih2YXIgaT0wO2k8bGVuZ3RoOysraSl7dmFyIGNoYXJDb2RlPXZhbHVlLmNoYXJDb2RlQXQoaSk7aWYoY2hhckNvZGU+MjU1KXtfZnJlZShiYXNlKTt0aHJvd0JpbmRpbmdFcnJvcigiU3RyaW5nIGhhcyBVVEYtMTYgY29kZSB1bml0cyB0aGF0IGRvIG5vdCBmaXQgaW4gOCBiaXRzIil9KGdyb3dNZW1WaWV3cygpLEhFQVBVOClbcHRyK2ldPWNoYXJDb2RlfX19ZWxzZXsoZ3Jvd01lbVZpZXdzKCksSEVBUFU4KS5zZXQodmFsdWUscHRyKX1pZihkZXN0cnVjdG9ycyE9PW51bGwpe2Rlc3RydWN0b3JzLnB1c2goX2ZyZWUsYmFzZSl9cmV0dXJuIGJhc2V9LHJlYWRWYWx1ZUZyb21Qb2ludGVyOnJlYWRQb2ludGVyLGRlc3RydWN0b3JGdW5jdGlvbihwdHIpe19mcmVlKHB0cil9fSl9O3ZhciBVVEYxNkRlY29kZXI9Z2xvYmFsVGhpcy5UZXh0RGVjb2Rlcj9uZXcgVGV4dERlY29kZXIoInV0Zi0xNmxlIik6dW5kZWZpbmVkO3ZhciBVVEYxNlRvU3RyaW5nPShwdHIsbWF4Qnl0ZXNUb1JlYWQsaWdub3JlTnVsKT0+e2Fzc2VydChwdHIlMj09MCwiUG9pbnRlciBwYXNzZWQgdG8gVVRGMTZUb1N0cmluZyBtdXN0IGJlIGFsaWduZWQgdG8gdHdvIGJ5dGVzISIpO3ZhciBpZHg9cHRyPj4xO3ZhciBlbmRJZHg9ZmluZFN0cmluZ0VuZCgoZ3Jvd01lbVZpZXdzKCksSEVBUFUxNiksaWR4LG1heEJ5dGVzVG9SZWFkLzIsaWdub3JlTnVsKTtpZihlbmRJZHgtaWR4PjE2JiZVVEYxNkRlY29kZXIpcmV0dXJuIFVURjE2RGVjb2Rlci5kZWNvZGUoKGdyb3dNZW1WaWV3cygpLEhFQVBVMTYpLnNsaWNlKGlkeCxlbmRJZHgpKTt2YXIgc3RyPSIiO2Zvcih2YXIgaT1pZHg7aTxlbmRJZHg7KytpKXt2YXIgY29kZVVuaXQ9KGdyb3dNZW1WaWV3cygpLEhFQVBVMTYpW2ldO3N0cis9U3RyaW5nLmZyb21DaGFyQ29kZShjb2RlVW5pdCl9cmV0dXJuIHN0cn07dmFyIHN0cmluZ1RvVVRGMTY9KHN0cixvdXRQdHIsbWF4Qnl0ZXNUb1dyaXRlKT0+e2Fzc2VydChvdXRQdHIlMj09MCwiUG9pbnRlciBwYXNzZWQgdG8gc3RyaW5nVG9VVEYxNiBtdXN0IGJlIGFsaWduZWQgdG8gdHdvIGJ5dGVzISIpO2Fzc2VydCh0eXBlb2YgbWF4Qnl0ZXNUb1dyaXRlPT0ibnVtYmVyIiwic3RyaW5nVG9VVEYxNihzdHIsIG91dFB0ciwgbWF4Qnl0ZXNUb1dyaXRlKSBpcyBtaXNzaW5nIHRoZSB0aGlyZCBwYXJhbWV0ZXIgdGhhdCBzcGVjaWZpZXMgdGhlIGxlbmd0aCBvZiB0aGUgb3V0cHV0IGJ1ZmZlciEiKTttYXhCeXRlc1RvV3JpdGU/Pz0yMTQ3NDgzNjQ3O2lmKG1heEJ5dGVzVG9Xcml0ZTwyKXJldHVybiAwO21heEJ5dGVzVG9Xcml0ZS09Mjt2YXIgc3RhcnRQdHI9b3V0UHRyO3ZhciBudW1DaGFyc1RvV3JpdGU9bWF4Qnl0ZXNUb1dyaXRlPHN0ci5sZW5ndGgqMj9tYXhCeXRlc1RvV3JpdGUvMjpzdHIubGVuZ3RoO2Zvcih2YXIgaT0wO2k8bnVtQ2hhcnNUb1dyaXRlOysraSl7dmFyIGNvZGVVbml0PXN0ci5jaGFyQ29kZUF0KGkpOyhncm93TWVtVmlld3MoKSxIRUFQMTYpW291dFB0cj4+MV09Y29kZVVuaXQ7b3V0UHRyKz0yfShncm93TWVtVmlld3MoKSxIRUFQMTYpW291dFB0cj4+MV09MDtyZXR1cm4gb3V0UHRyLXN0YXJ0UHRyfTt2YXIgbGVuZ3RoQnl0ZXNVVEYxNj1zdHI9PnN0ci5sZW5ndGgqMjt2YXIgVVRGMzJUb1N0cmluZz0ocHRyLG1heEJ5dGVzVG9SZWFkLGlnbm9yZU51bCk9Pnthc3NlcnQocHRyJTQ9PTAsIlBvaW50ZXIgcGFzc2VkIHRvIFVURjMyVG9TdHJpbmcgbXVzdCBiZSBhbGlnbmVkIHRvIGZvdXIgYnl0ZXMhIik7dmFyIHN0cj0iIjt2YXIgc3RhcnRJZHg9cHRyPj4yO2Zvcih2YXIgaT0wOyEoaT49bWF4Qnl0ZXNUb1JlYWQvNCk7aSsrKXt2YXIgdXRmMzI9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3N0YXJ0SWR4K2ldO2lmKCF1dGYzMiYmIWlnbm9yZU51bClicmVhaztzdHIrPVN0cmluZy5mcm9tQ29kZVBvaW50KHV0ZjMyKX1yZXR1cm4gc3RyfTt2YXIgc3RyaW5nVG9VVEYzMj0oc3RyLG91dFB0cixtYXhCeXRlc1RvV3JpdGUpPT57YXNzZXJ0KG91dFB0ciU0PT0wLCJQb2ludGVyIHBhc3NlZCB0byBzdHJpbmdUb1VURjMyIG11c3QgYmUgYWxpZ25lZCB0byBmb3VyIGJ5dGVzISIpO2Fzc2VydCh0eXBlb2YgbWF4Qnl0ZXNUb1dyaXRlPT0ibnVtYmVyIiwic3RyaW5nVG9VVEYzMihzdHIsIG91dFB0ciwgbWF4Qnl0ZXNUb1dyaXRlKSBpcyBtaXNzaW5nIHRoZSB0aGlyZCBwYXJhbWV0ZXIgdGhhdCBzcGVjaWZpZXMgdGhlIGxlbmd0aCBvZiB0aGUgb3V0cHV0IGJ1ZmZlciEiKTttYXhCeXRlc1RvV3JpdGU/Pz0yMTQ3NDgzNjQ3O2lmKG1heEJ5dGVzVG9Xcml0ZTw0KXJldHVybiAwO3ZhciBzdGFydFB0cj1vdXRQdHI7dmFyIGVuZFB0cj1zdGFydFB0cittYXhCeXRlc1RvV3JpdGUtNDtmb3IodmFyIGk9MDtpPHN0ci5sZW5ndGg7KytpKXt2YXIgY29kZVBvaW50PXN0ci5jb2RlUG9pbnRBdChpKTtpZihjb2RlUG9pbnQ+NjU1MzUpe2krK30oZ3Jvd01lbVZpZXdzKCksSEVBUDMyKVtvdXRQdHI+PjJdPWNvZGVQb2ludDtvdXRQdHIrPTQ7aWYob3V0UHRyKzQ+ZW5kUHRyKWJyZWFrfShncm93TWVtVmlld3MoKSxIRUFQMzIpW291dFB0cj4+Ml09MDtyZXR1cm4gb3V0UHRyLXN0YXJ0UHRyfTt2YXIgbGVuZ3RoQnl0ZXNVVEYzMj1zdHI9Pnt2YXIgbGVuPTA7Zm9yKHZhciBpPTA7aTxzdHIubGVuZ3RoOysraSl7dmFyIGNvZGVQb2ludD1zdHIuY29kZVBvaW50QXQoaSk7aWYoY29kZVBvaW50PjY1NTM1KXtpKyt9bGVuKz00fXJldHVybiBsZW59O3ZhciBfX2VtYmluZF9yZWdpc3Rlcl9zdGRfd3N0cmluZz0ocmF3VHlwZSxjaGFyU2l6ZSxuYW1lKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTt2YXIgZGVjb2RlU3RyaW5nLGVuY29kZVN0cmluZyxsZW5ndGhCeXRlc1VURjtpZihjaGFyU2l6ZT09PTIpe2RlY29kZVN0cmluZz1VVEYxNlRvU3RyaW5nO2VuY29kZVN0cmluZz1zdHJpbmdUb1VURjE2O2xlbmd0aEJ5dGVzVVRGPWxlbmd0aEJ5dGVzVVRGMTZ9ZWxzZXthc3NlcnQoY2hhclNpemU9PT00LCJvbmx5IDItYnl0ZSBhbmQgNC1ieXRlIHN0cmluZ3MgYXJlIGN1cnJlbnRseSBzdXBwb3J0ZWQiKTtkZWNvZGVTdHJpbmc9VVRGMzJUb1N0cmluZztlbmNvZGVTdHJpbmc9c3RyaW5nVG9VVEYzMjtsZW5ndGhCeXRlc1VURj1sZW5ndGhCeXRlc1VURjMyfXJlZ2lzdGVyVHlwZShyYXdUeXBlLHtuYW1lLGZyb21XaXJlVHlwZTp2YWx1ZT0+e3ZhciBsZW5ndGg9KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3ZhbHVlPj4yXTt2YXIgc3RyPWRlY29kZVN0cmluZyh2YWx1ZSs0LGxlbmd0aCpjaGFyU2l6ZSx0cnVlKTtfZnJlZSh2YWx1ZSk7cmV0dXJuIHN0cn0sdG9XaXJlVHlwZTooZGVzdHJ1Y3RvcnMsdmFsdWUpPT57aWYoISh0eXBlb2YgdmFsdWU9PSJzdHJpbmciKSl7dGhyb3dCaW5kaW5nRXJyb3IoYENhbm5vdCBwYXNzIG5vbi1zdHJpbmcgdG8gQysrIHN0cmluZyB0eXBlICR7bmFtZX1gKX12YXIgbGVuZ3RoPWxlbmd0aEJ5dGVzVVRGKHZhbHVlKTt2YXIgcHRyPV9tYWxsb2MoNCtsZW5ndGgrY2hhclNpemUpOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtwdHI+PjJdPWxlbmd0aC9jaGFyU2l6ZTtlbmNvZGVTdHJpbmcodmFsdWUscHRyKzQsbGVuZ3RoK2NoYXJTaXplKTtpZihkZXN0cnVjdG9ycyE9PW51bGwpe2Rlc3RydWN0b3JzLnB1c2goX2ZyZWUscHRyKX1yZXR1cm4gcHRyfSxyZWFkVmFsdWVGcm9tUG9pbnRlcjpyZWFkUG9pbnRlcixkZXN0cnVjdG9yRnVuY3Rpb24ocHRyKXtfZnJlZShwdHIpfX0pfTt2YXIgX19lbWJpbmRfcmVnaXN0ZXJfdm9pZD0ocmF3VHlwZSxuYW1lKT0+e25hbWU9QXNjaWlUb1N0cmluZyhuYW1lKTtyZWdpc3RlclR5cGUocmF3VHlwZSx7aXNWb2lkOnRydWUsbmFtZSxmcm9tV2lyZVR5cGU6KCk9PnVuZGVmaW5lZCx0b1dpcmVUeXBlOihkZXN0cnVjdG9ycyxvKT0+dW5kZWZpbmVkfSl9O3ZhciBfX2Vtc2NyaXB0ZW5faW5pdF9tYWluX3RocmVhZF9qcz10Yj0+e19fZW1zY3JpcHRlbl90aHJlYWRfaW5pdCh0YiwhRU5WSVJPTk1FTlRfSVNfV09SS0VSLDEsIUVOVklST05NRU5UX0lTX1dFQiw2NTUzNixmYWxzZSk7UFRocmVhZC50aHJlYWRJbml0VExTKCl9O3ZhciBfX2Vtc2NyaXB0ZW5fdGhyZWFkX21haWxib3hfYXdhaXQ9cHRocmVhZF9wdHI9PntpZihBdG9taWNzLndhaXRBc3luYyl7dmFyIHdhaXQ9QXRvbWljcy53YWl0QXN5bmMoKGdyb3dNZW1WaWV3cygpLEhFQVAzMikscHRocmVhZF9wdHI+PjIscHRocmVhZF9wdHIpO2Fzc2VydCh3YWl0LmFzeW5jKTt3YWl0LnZhbHVlLnRoZW4oY2hlY2tNYWlsYm94KTt2YXIgd2FpdGluZ0FzeW5jPXB0aHJlYWRfcHRyKzEyODtBdG9taWNzLnN0b3JlKChncm93TWVtVmlld3MoKSxIRUFQMzIpLHdhaXRpbmdBc3luYz4+MiwxKX19O3ZhciBjaGVja01haWxib3g9KCk9PmNhbGxVc2VyQ2FsbGJhY2soKCk9Pnt2YXIgcHRocmVhZF9wdHI9X3B0aHJlYWRfc2VsZigpO2lmKHB0aHJlYWRfcHRyKXtfX2Vtc2NyaXB0ZW5fdGhyZWFkX21haWxib3hfYXdhaXQocHRocmVhZF9wdHIpO19fZW1zY3JpcHRlbl9jaGVja19tYWlsYm94KCl9fSk7dmFyIF9fZW1zY3JpcHRlbl9ub3RpZnlfbWFpbGJveF9wb3N0bWVzc2FnZT0odGFyZ2V0VGhyZWFkLGN1cnJUaHJlYWRJZCk9PntpZih0YXJnZXRUaHJlYWQ9PWN1cnJUaHJlYWRJZCl7c2V0VGltZW91dChjaGVja01haWxib3gpfWVsc2UgaWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCl7cG9zdE1lc3NhZ2Uoe3RhcmdldFRocmVhZCxjbWQ6ImNoZWNrTWFpbGJveCJ9KX1lbHNle3ZhciB3b3JrZXI9UFRocmVhZC5wdGhyZWFkc1t0YXJnZXRUaHJlYWRdO2lmKCF3b3JrZXIpe2VycihgQ2Fubm90IHNlbmQgbWVzc2FnZSB0byB0aHJlYWQgd2l0aCBJRCAke3RhcmdldFRocmVhZH0sIHVua25vd24gdGhyZWFkIElEIWApO3JldHVybn13b3JrZXIucG9zdE1lc3NhZ2Uoe2NtZDoiY2hlY2tNYWlsYm94In0pfX07dmFyIHByb3hpZWRKU0NhbGxBcmdzPVtdO3ZhciBfX2Vtc2NyaXB0ZW5fcmVjZWl2ZV9vbl9tYWluX3RocmVhZF9qcz0oZnVuY0luZGV4LGVtQXNtQWRkcixjYWxsaW5nVGhyZWFkLG51bUNhbGxBcmdzLGFyZ3MpPT57bnVtQ2FsbEFyZ3MvPTI7cHJveGllZEpTQ2FsbEFyZ3MubGVuZ3RoPW51bUNhbGxBcmdzO3ZhciBiPWFyZ3M+PjM7Zm9yKHZhciBpPTA7aTxudW1DYWxsQXJncztpKyspe2lmKChncm93TWVtVmlld3MoKSxIRUFQNjQpW2IrMippXSl7cHJveGllZEpTQ2FsbEFyZ3NbaV09KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYisyKmkrMV19ZWxzZXtwcm94aWVkSlNDYWxsQXJnc1tpXT0oZ3Jvd01lbVZpZXdzKCksSEVBUEY2NClbYisyKmkrMV19fXZhciBmdW5jPWVtQXNtQWRkcj9BU01fQ09OU1RTW2VtQXNtQWRkcl06cHJveGllZEZ1bmN0aW9uVGFibGVbZnVuY0luZGV4XTthc3NlcnQoIShmdW5jSW5kZXgmJmVtQXNtQWRkcikpO2Fzc2VydChmdW5jLmxlbmd0aD09bnVtQ2FsbEFyZ3MsIkNhbGwgYXJncyBtaXNtYXRjaCBpbiBfZW1zY3JpcHRlbl9yZWNlaXZlX29uX21haW5fdGhyZWFkX2pzIik7UFRocmVhZC5jdXJyZW50UHJveGllZE9wZXJhdGlvbkNhbGxlclRocmVhZD1jYWxsaW5nVGhyZWFkO3ZhciBydG49ZnVuYyguLi5wcm94aWVkSlNDYWxsQXJncyk7UFRocmVhZC5jdXJyZW50UHJveGllZE9wZXJhdGlvbkNhbGxlclRocmVhZD0wO2Fzc2VydCh0eXBlb2YgcnRuIT0iYmlnaW50Iik7cmV0dXJuIHJ0bn07dmFyIF9fZW1zY3JpcHRlbl90aHJlYWRfY2xlYW51cD10aHJlYWQ9PntpZighRU5WSVJPTk1FTlRfSVNfUFRIUkVBRCljbGVhbnVwVGhyZWFkKHRocmVhZCk7ZWxzZSBwb3N0TWVzc2FnZSh7Y21kOiJjbGVhbnVwVGhyZWFkIix0aHJlYWR9KX07dmFyIF9fZW1zY3JpcHRlbl90aHJlYWRfc2V0X3N0cm9uZ3JlZj10aHJlYWQ9Pnt9O3ZhciBJTlQ1M19NQVg9OTAwNzE5OTI1NDc0MDk5Mjt2YXIgSU5UNTNfTUlOPS05MDA3MTk5MjU0NzQwOTkyO3ZhciBiaWdpbnRUb0k1M0NoZWNrZWQ9bnVtPT5udW08SU5UNTNfTUlOfHxudW0+SU5UNTNfTUFYP05hTjpOdW1iZXIobnVtKTtmdW5jdGlvbiBfX21tYXBfanMobGVuLHByb3QsZmxhZ3MsZmQsb2Zmc2V0LGFsbG9jYXRlZCxhZGRyKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxMCwwLDEsbGVuLHByb3QsZmxhZ3MsZmQsb2Zmc2V0LGFsbG9jYXRlZCxhZGRyKTtvZmZzZXQ9YmlnaW50VG9JNTNDaGVja2VkKG9mZnNldCk7dHJ5e2Fzc2VydCghaXNOYU4ob2Zmc2V0KSk7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO3ZhciByZXM9RlMubW1hcChzdHJlYW0sbGVuLG9mZnNldCxwcm90LGZsYWdzKTt2YXIgcHRyPXJlcy5wdHI7KGdyb3dNZW1WaWV3cygpLEhFQVAzMilbYWxsb2NhdGVkPj4yXT1yZXMuYWxsb2NhdGVkOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVthZGRyPj4yXT1wdHI7cmV0dXJuIDB9Y2F0Y2goZSl7aWYodHlwZW9mIEZTPT0idW5kZWZpbmVkInx8IShlLm5hbWU9PT0iRXJybm9FcnJvciIpKXRocm93IGU7cmV0dXJuLWUuZXJybm99fWZ1bmN0aW9uIF9fbXVubWFwX2pzKGFkZHIsbGVuLHByb3QsZmxhZ3MsZmQsb2Zmc2V0KXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxMSwwLDEsYWRkcixsZW4scHJvdCxmbGFncyxmZCxvZmZzZXQpO29mZnNldD1iaWdpbnRUb0k1M0NoZWNrZWQob2Zmc2V0KTt0cnl7dmFyIHN0cmVhbT1TWVNDQUxMUy5nZXRTdHJlYW1Gcm9tRkQoZmQpO2lmKHByb3QmMil7U1lTQ0FMTFMuZG9Nc3luYyhhZGRyLHN0cmVhbSxsZW4sZmxhZ3Msb2Zmc2V0KX19Y2F0Y2goZSl7aWYodHlwZW9mIEZTPT0idW5kZWZpbmVkInx8IShlLm5hbWU9PT0iRXJybm9FcnJvciIpKXRocm93IGU7cmV0dXJuLWUuZXJybm99fXZhciBfX3R6c2V0X2pzPSh0aW1lem9uZSxkYXlsaWdodCxzdGRfbmFtZSxkc3RfbmFtZSk9Pnt2YXIgY3VycmVudFllYXI9KG5ldyBEYXRlKS5nZXRGdWxsWWVhcigpO3ZhciB3aW50ZXI9bmV3IERhdGUoY3VycmVudFllYXIsMCwxKTt2YXIgc3VtbWVyPW5ldyBEYXRlKGN1cnJlbnRZZWFyLDYsMSk7dmFyIHdpbnRlck9mZnNldD13aW50ZXIuZ2V0VGltZXpvbmVPZmZzZXQoKTt2YXIgc3VtbWVyT2Zmc2V0PXN1bW1lci5nZXRUaW1lem9uZU9mZnNldCgpO3ZhciBzdGRUaW1lem9uZU9mZnNldD1NYXRoLm1heCh3aW50ZXJPZmZzZXQsc3VtbWVyT2Zmc2V0KTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbdGltZXpvbmU+PjJdPXN0ZFRpbWV6b25lT2Zmc2V0KjYwOyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2RheWxpZ2h0Pj4yXT1OdW1iZXIod2ludGVyT2Zmc2V0IT1zdW1tZXJPZmZzZXQpO3ZhciBleHRyYWN0Wm9uZT10aW1lem9uZU9mZnNldD0+e3ZhciBzaWduPXRpbWV6b25lT2Zmc2V0Pj0wPyItIjoiKyI7dmFyIGFic09mZnNldD1NYXRoLmFicyh0aW1lem9uZU9mZnNldCk7dmFyIGhvdXJzPVN0cmluZyhNYXRoLmZsb29yKGFic09mZnNldC82MCkpLnBhZFN0YXJ0KDIsIjAiKTt2YXIgbWludXRlcz1TdHJpbmcoYWJzT2Zmc2V0JTYwKS5wYWRTdGFydCgyLCIwIik7cmV0dXJuYFVUQyR7c2lnbn0ke2hvdXJzfSR7bWludXRlc31gfTt2YXIgd2ludGVyTmFtZT1leHRyYWN0Wm9uZSh3aW50ZXJPZmZzZXQpO3ZhciBzdW1tZXJOYW1lPWV4dHJhY3Rab25lKHN1bW1lck9mZnNldCk7YXNzZXJ0KHdpbnRlck5hbWUpO2Fzc2VydChzdW1tZXJOYW1lKTthc3NlcnQobGVuZ3RoQnl0ZXNVVEY4KHdpbnRlck5hbWUpPD0xNixgdGltZXpvbmUgbmFtZSB0cnVuY2F0ZWQgdG8gZml0IGluIFRaTkFNRV9NQVggKCR7d2ludGVyTmFtZX0pYCk7YXNzZXJ0KGxlbmd0aEJ5dGVzVVRGOChzdW1tZXJOYW1lKTw9MTYsYHRpbWV6b25lIG5hbWUgdHJ1bmNhdGVkIHRvIGZpdCBpbiBUWk5BTUVfTUFYICgke3N1bW1lck5hbWV9KWApO2lmKHN1bW1lck9mZnNldDx3aW50ZXJPZmZzZXQpe3N0cmluZ1RvVVRGOCh3aW50ZXJOYW1lLHN0ZF9uYW1lLDE3KTtzdHJpbmdUb1VURjgoc3VtbWVyTmFtZSxkc3RfbmFtZSwxNyl9ZWxzZXtzdHJpbmdUb1VURjgod2ludGVyTmFtZSxkc3RfbmFtZSwxNyk7c3RyaW5nVG9VVEY4KHN1bW1lck5hbWUsc3RkX25hbWUsMTcpfX07dmFyIF9lbXNjcmlwdGVuX2dldF9ub3c9KCk9PnBlcmZvcm1hbmNlLnRpbWVPcmlnaW4rcGVyZm9ybWFuY2Uubm93KCk7dmFyIF9lbXNjcmlwdGVuX2RhdGVfbm93PSgpPT5EYXRlLm5vdygpO3ZhciBub3dJc01vbm90b25pYz0xO3ZhciBjaGVja1dhc2lDbG9jaz1jbG9ja19pZD0+Y2xvY2tfaWQ+PTAmJmNsb2NrX2lkPD0zO2Z1bmN0aW9uIF9jbG9ja190aW1lX2dldChjbGtfaWQsaWdub3JlZF9wcmVjaXNpb24scHRpbWUpe2lnbm9yZWRfcHJlY2lzaW9uPWJpZ2ludFRvSTUzQ2hlY2tlZChpZ25vcmVkX3ByZWNpc2lvbik7aWYoIWNoZWNrV2FzaUNsb2NrKGNsa19pZCkpe3JldHVybiAyOH12YXIgbm93O2lmKGNsa19pZD09PTApe25vdz1fZW1zY3JpcHRlbl9kYXRlX25vdygpfWVsc2UgaWYobm93SXNNb25vdG9uaWMpe25vdz1fZW1zY3JpcHRlbl9nZXRfbm93KCl9ZWxzZXtyZXR1cm4gNTJ9dmFyIG5zZWM9TWF0aC5yb3VuZChub3cqMWUzKjFlMyk7KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbcHRpbWU+PjNdPUJpZ0ludChuc2VjKTtyZXR1cm4gMH12YXIgcmVhZEVtQXNtQXJnc0FycmF5PVtdO3ZhciByZWFkRW1Bc21BcmdzPShzaWdQdHIsYnVmKT0+e2Fzc2VydChBcnJheS5pc0FycmF5KHJlYWRFbUFzbUFyZ3NBcnJheSkpO2Fzc2VydChidWYlMTY9PTApO3JlYWRFbUFzbUFyZ3NBcnJheS5sZW5ndGg9MDt2YXIgY2g7d2hpbGUoY2g9KGdyb3dNZW1WaWV3cygpLEhFQVBVOClbc2lnUHRyKytdKXt2YXIgY2hyPVN0cmluZy5mcm9tQ2hhckNvZGUoY2gpO3ZhciB2YWxpZENoYXJzPVsiZCIsImYiLCJpIiwicCJdO3ZhbGlkQ2hhcnMucHVzaCgiaiIpO2Fzc2VydCh2YWxpZENoYXJzLmluY2x1ZGVzKGNociksYEludmFsaWQgY2hhcmFjdGVyICR7Y2h9KCIke2Nocn0iKSBpbiByZWFkRW1Bc21BcmdzISBVc2Ugb25seSBbJHt2YWxpZENoYXJzfV0sIGFuZCBkbyBub3Qgc3BlY2lmeSAidiIgZm9yIHZvaWQgcmV0dXJuIGFyZ3VtZW50LmApO3ZhciB3aWRlPWNoIT0xMDU7d2lkZSY9Y2ghPTExMjtidWYrPXdpZGUmJmJ1ZiU4PzQ6MDtyZWFkRW1Bc21BcmdzQXJyYXkucHVzaChjaD09MTEyPyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtidWY+PjJdOmNoPT0xMDY/KGdyb3dNZW1WaWV3cygpLEhFQVA2NClbYnVmPj4zXTpjaD09MTA1Pyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2J1Zj4+Ml06KGdyb3dNZW1WaWV3cygpLEhFQVBGNjQpW2J1Zj4+M10pO2J1Zis9d2lkZT84OjR9cmV0dXJuIHJlYWRFbUFzbUFyZ3NBcnJheX07dmFyIHJ1bkVtQXNtRnVuY3Rpb249KGNvZGUsc2lnUHRyLGFyZ2J1Zik9Pnt2YXIgYXJncz1yZWFkRW1Bc21BcmdzKHNpZ1B0cixhcmdidWYpO2Fzc2VydChBU01fQ09OU1RTLmhhc093blByb3BlcnR5KGNvZGUpLGBObyBFTV9BU00gY29uc3RhbnQgZm91bmQgYXQgYWRkcmVzcyAke2NvZGV9LiAgVGhlIGxvYWRlZCBXZWJBc3NlbWJseSBmaWxlIGlzIGxpa2VseSBvdXQgb2Ygc3luYyB3aXRoIHRoZSBnZW5lcmF0ZWQgSmF2YVNjcmlwdC5gKTtyZXR1cm4gQVNNX0NPTlNUU1tjb2RlXSguLi5hcmdzKX07dmFyIF9lbXNjcmlwdGVuX2FzbV9jb25zdF9pbnQ9KGNvZGUsc2lnUHRyLGFyZ2J1Zik9PnJ1bkVtQXNtRnVuY3Rpb24oY29kZSxzaWdQdHIsYXJnYnVmKTt2YXIgX2Vtc2NyaXB0ZW5fY2hlY2tfYmxvY2tpbmdfYWxsb3dlZD0oKT0+e2lmKEVOVklST05NRU5UX0lTX1dPUktFUilyZXR1cm47d2Fybk9uY2UoIkJsb2NraW5nIG9uIHRoZSBtYWluIHRocmVhZCBpcyB2ZXJ5IGRhbmdlcm91cywgc2VlIGh0dHBzOi8vZW1zY3JpcHRlbi5vcmcvZG9jcy9wb3J0aW5nL3B0aHJlYWRzLmh0bWwjYmxvY2tpbmctb24tdGhlLW1haW4tYnJvd3Nlci10aHJlYWQiKX07dmFyIF9lbXNjcmlwdGVuX2Vycm49KHN0cixsZW4pPT5lcnIoVVRGOFRvU3RyaW5nKHN0cixsZW4pKTt2YXIgX2Vtc2NyaXB0ZW5fZXhpdF93aXRoX2xpdmVfcnVudGltZT0oKT0+e3J1bnRpbWVLZWVwYWxpdmVQdXNoKCk7dGhyb3cidW53aW5kIn07dmFyIGdldEhlYXBNYXg9KCk9PjIxNDc0ODM2NDg7dmFyIF9lbXNjcmlwdGVuX2dldF9oZWFwX21heD0oKT0+Z2V0SGVhcE1heCgpO3ZhciBfZW1zY3JpcHRlbl9udW1fbG9naWNhbF9jb3Jlcz0oKT0+bmF2aWdhdG9yWyJoYXJkd2FyZUNvbmN1cnJlbmN5Il07dmFyIFVOV0lORF9DQUNIRT17fTt2YXIgc3RyaW5nVG9OZXdVVEY4PXN0cj0+e3ZhciBzaXplPWxlbmd0aEJ5dGVzVVRGOChzdHIpKzE7dmFyIHJldD1fbWFsbG9jKHNpemUpO2lmKHJldClzdHJpbmdUb1VURjgoc3RyLHJldCxzaXplKTtyZXR1cm4gcmV0fTt2YXIgY29udmVydEZyYW1lVG9QQz1mcmFtZT0+e3ZhciBtYXRjaDtpZihtYXRjaD0vXGJ3YXNtLWZ1bmN0aW9uXFtcZCtcXTooMHhbMC05YS1mXSspLy5leGVjKGZyYW1lKSl7cmV0dXJuK21hdGNoWzFdfWVsc2UgaWYobWF0Y2g9L1xid2FzbS1mdW5jdGlvblxbKFxkKylcXTooXGQrKS8uZXhlYyhmcmFtZSkpe3dhcm5PbmNlKCJsZWdhY3kgYmFja3RyYWNlIGZvcm1hdCBkZXRlY3RlZCwgdGhpcyB2ZXJzaW9uIG9mIHY4IGlzIG5vIGxvbmdlciBzdXBwb3J0ZWQgYnkgdGhlIGVtc2NyaXB0ZW4gYmFja3RyYWNlIG1lY2hhbmlzbSIpfWVsc2UgaWYobWF0Y2g9LzooXGQrKTpcZCsoPzpcKXwkKS8uZXhlYyhmcmFtZSkpe3JldHVybiAyMTQ3NDgzNjQ4fCttYXRjaFsxXX1yZXR1cm4gMH07dmFyIHNhdmVJblVud2luZENhY2hlPWNhbGxzdGFjaz0+e2Zvcih2YXIgbGluZSBvZiBjYWxsc3RhY2spe3ZhciBwYz1jb252ZXJ0RnJhbWVUb1BDKGxpbmUpO2lmKHBjKXtVTldJTkRfQ0FDSEVbcGNdPWxpbmV9fX07dmFyIGpzU3RhY2tUcmFjZT0oKT0+KG5ldyBFcnJvcikuc3RhY2sudG9TdHJpbmcoKTt2YXIgX2Vtc2NyaXB0ZW5fc3RhY2tfc25hcHNob3Q9KCk9Pnt2YXIgY2FsbHN0YWNrPWpzU3RhY2tUcmFjZSgpLnNwbGl0KCJcbiIpO2lmKGNhbGxzdGFja1swXT09IkVycm9yIil7Y2FsbHN0YWNrLnNoaWZ0KCl9c2F2ZUluVW53aW5kQ2FjaGUoY2FsbHN0YWNrKTtVTldJTkRfQ0FDSEUubGFzdF9hZGRyPWNvbnZlcnRGcmFtZVRvUEMoY2FsbHN0YWNrWzNdKTtVTldJTkRfQ0FDSEUubGFzdF9zdGFjaz1jYWxsc3RhY2s7cmV0dXJuIFVOV0lORF9DQUNIRS5sYXN0X2FkZHJ9O3ZhciBfZW1zY3JpcHRlbl9wY19nZXRfZnVuY3Rpb249cGM9Pnt2YXIgZnJhbWU9VU5XSU5EX0NBQ0hFW3BjXTtpZighZnJhbWUpcmV0dXJuIDA7dmFyIG5hbWU7dmFyIG1hdGNoO2lmKG1hdGNoPS9eXHMrYXQgLipcLndhc21cLiguKikgXCguKlwpJC8uZXhlYyhmcmFtZSkpe25hbWU9bWF0Y2hbMV19ZWxzZSBpZihtYXRjaD0vXlxzK2F0ICguKikgXCguKlwpJC8uZXhlYyhmcmFtZSkpe25hbWU9bWF0Y2hbMV19ZWxzZSBpZihtYXRjaD0vXiguKz8pQC8uZXhlYyhmcmFtZSkpe25hbWU9bWF0Y2hbMV19ZWxzZXtyZXR1cm4gMH1fZnJlZShfZW1zY3JpcHRlbl9wY19nZXRfZnVuY3Rpb24ucmV0Pz8wKTtfZW1zY3JpcHRlbl9wY19nZXRfZnVuY3Rpb24ucmV0PXN0cmluZ1RvTmV3VVRGOChuYW1lKTtyZXR1cm4gX2Vtc2NyaXB0ZW5fcGNfZ2V0X2Z1bmN0aW9uLnJldH07dmFyIGdyb3dNZW1vcnk9c2l6ZT0+e3ZhciBvbGRIZWFwU2l6ZT13YXNtTWVtb3J5LmJ1ZmZlci5ieXRlTGVuZ3RoO3ZhciBwYWdlcz0oc2l6ZS1vbGRIZWFwU2l6ZSs2NTUzNSkvNjU1MzZ8MDt0cnl7d2FzbU1lbW9yeS5ncm93KHBhZ2VzKTt1cGRhdGVNZW1vcnlWaWV3cygpO3JldHVybiAxfWNhdGNoKGUpe2VycihgZ3Jvd01lbW9yeTogQXR0ZW1wdGVkIHRvIGdyb3cgaGVhcCBmcm9tICR7b2xkSGVhcFNpemV9IGJ5dGVzIHRvICR7c2l6ZX0gYnl0ZXMsIGJ1dCBnb3QgZXJyb3I6ICR7ZX1gKX19O3ZhciBfZW1zY3JpcHRlbl9yZXNpemVfaGVhcD1yZXF1ZXN0ZWRTaXplPT57dmFyIG9sZFNpemU9KGdyb3dNZW1WaWV3cygpLEhFQVBVOCkubGVuZ3RoO3JlcXVlc3RlZFNpemU+Pj49MDtpZihyZXF1ZXN0ZWRTaXplPD1vbGRTaXplKXtyZXR1cm4gZmFsc2V9dmFyIG1heEhlYXBTaXplPWdldEhlYXBNYXgoKTtpZihyZXF1ZXN0ZWRTaXplPm1heEhlYXBTaXplKXtlcnIoYENhbm5vdCBlbmxhcmdlIG1lbW9yeSwgcmVxdWVzdGVkICR7cmVxdWVzdGVkU2l6ZX0gYnl0ZXMsIGJ1dCB0aGUgbGltaXQgaXMgJHttYXhIZWFwU2l6ZX0gYnl0ZXMhYCk7cmV0dXJuIGZhbHNlfWZvcih2YXIgY3V0RG93bj0xO2N1dERvd248PTQ7Y3V0RG93bio9Mil7dmFyIG92ZXJHcm93bkhlYXBTaXplPW9sZFNpemUqKDErLjIvY3V0RG93bik7b3Zlckdyb3duSGVhcFNpemU9TWF0aC5taW4ob3Zlckdyb3duSGVhcFNpemUscmVxdWVzdGVkU2l6ZSsxMDA2NjMyOTYpO3ZhciBuZXdTaXplPU1hdGgubWluKG1heEhlYXBTaXplLGFsaWduTWVtb3J5KE1hdGgubWF4KHJlcXVlc3RlZFNpemUsb3Zlckdyb3duSGVhcFNpemUpLDY1NTM2KSk7dmFyIHJlcGxhY2VtZW50PWdyb3dNZW1vcnkobmV3U2l6ZSk7aWYocmVwbGFjZW1lbnQpe3JldHVybiB0cnVlfX1lcnIoYEZhaWxlZCB0byBncm93IHRoZSBoZWFwIGZyb20gJHtvbGRTaXplfSBieXRlcyB0byAke25ld1NpemV9IGJ5dGVzLCBub3QgZW5vdWdoIG1lbW9yeSFgKTtyZXR1cm4gZmFsc2V9O3ZhciBfZW1zY3JpcHRlbl9zdGFja191bndpbmRfYnVmZmVyPShhZGRyLGJ1ZmZlcixjb3VudCk9Pnt2YXIgc3RhY2s7aWYoVU5XSU5EX0NBQ0hFLmxhc3RfYWRkcj09YWRkcil7c3RhY2s9VU5XSU5EX0NBQ0hFLmxhc3Rfc3RhY2t9ZWxzZXtzdGFjaz1qc1N0YWNrVHJhY2UoKS5zcGxpdCgiXG4iKTtpZihzdGFja1swXT09IkVycm9yIil7c3RhY2suc2hpZnQoKX1zYXZlSW5VbndpbmRDYWNoZShzdGFjayl9dmFyIG9mZnNldD0zO3doaWxlKHN0YWNrW29mZnNldF0mJmNvbnZlcnRGcmFtZVRvUEMoc3RhY2tbb2Zmc2V0XSkhPWFkZHIpeysrb2Zmc2V0fWZvcih2YXIgaT0wO2k8Y291bnQmJnN0YWNrW2krb2Zmc2V0XTsrK2kpeyhncm93TWVtVmlld3MoKSxIRUFQMzIpW2J1ZmZlcitpKjQ+PjJdPWNvbnZlcnRGcmFtZVRvUEMoc3RhY2tbaStvZmZzZXRdKX1yZXR1cm4gaX07dmFyIEVOVj17fTt2YXIgZ2V0RXhlY3V0YWJsZU5hbWU9KCk9PnRoaXNQcm9ncmFtfHwiLi90aGlzLnByb2dyYW0iO3ZhciBnZXRFbnZTdHJpbmdzPSgpPT57aWYoIWdldEVudlN0cmluZ3Muc3RyaW5ncyl7dmFyIGxhbmc9KHR5cGVvZiBuYXZpZ2F0b3I9PSJvYmplY3QiJiZuYXZpZ2F0b3IubGFuZ3VhZ2V8fCJDIikucmVwbGFjZSgiLSIsIl8iKSsiLlVURi04Ijt2YXIgZW52PXtVU0VSOiJ3ZWJfdXNlciIsTE9HTkFNRToid2ViX3VzZXIiLFBBVEg6Ii8iLFBXRDoiLyIsSE9NRToiL2hvbWUvd2ViX3VzZXIiLExBTkc6bGFuZyxfOmdldEV4ZWN1dGFibGVOYW1lKCl9O2Zvcih2YXIgeCBpbiBFTlYpe2lmKEVOVlt4XT09PXVuZGVmaW5lZClkZWxldGUgZW52W3hdO2Vsc2UgZW52W3hdPUVOVlt4XX12YXIgc3RyaW5ncz1bXTtmb3IodmFyIHggaW4gZW52KXtzdHJpbmdzLnB1c2goYCR7eH09JHtlbnZbeF19YCl9Z2V0RW52U3RyaW5ncy5zdHJpbmdzPXN0cmluZ3N9cmV0dXJuIGdldEVudlN0cmluZ3Muc3RyaW5nc307ZnVuY3Rpb24gX2Vudmlyb25fZ2V0KF9fZW52aXJvbixlbnZpcm9uX2J1Zil7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMTIsMCwxLF9fZW52aXJvbixlbnZpcm9uX2J1Zik7dmFyIGJ1ZlNpemU9MDt2YXIgZW52cD0wO2Zvcih2YXIgc3RyaW5nIG9mIGdldEVudlN0cmluZ3MoKSl7dmFyIHB0cj1lbnZpcm9uX2J1ZitidWZTaXplOyhncm93TWVtVmlld3MoKSxIRUFQVTMyKVtfX2Vudmlyb24rZW52cD4+Ml09cHRyO2J1ZlNpemUrPXN0cmluZ1RvVVRGOChzdHJpbmcscHRyLEluZmluaXR5KSsxO2VudnArPTR9cmV0dXJuIDB9ZnVuY3Rpb24gX2Vudmlyb25fc2l6ZXNfZ2V0KHBlbnZpcm9uX2NvdW50LHBlbnZpcm9uX2J1Zl9zaXplKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxMywwLDEscGVudmlyb25fY291bnQscGVudmlyb25fYnVmX3NpemUpO3ZhciBzdHJpbmdzPWdldEVudlN0cmluZ3MoKTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcGVudmlyb25fY291bnQ+PjJdPXN0cmluZ3MubGVuZ3RoO3ZhciBidWZTaXplPTA7Zm9yKHZhciBzdHJpbmcgb2Ygc3RyaW5ncyl7YnVmU2l6ZSs9bGVuZ3RoQnl0ZXNVVEY4KHN0cmluZykrMX0oZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcGVudmlyb25fYnVmX3NpemU+PjJdPWJ1ZlNpemU7cmV0dXJuIDB9ZnVuY3Rpb24gX2ZkX2Nsb3NlKGZkKXtpZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXJldHVybiBwcm94eVRvTWFpblRocmVhZCgxNCwwLDEsZmQpO3RyeXt2YXIgc3RyZWFtPVNZU0NBTExTLmdldFN0cmVhbUZyb21GRChmZCk7RlMuY2xvc2Uoc3RyZWFtKTtyZXR1cm4gMH1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4gZS5lcnJub319dmFyIGRvUmVhZHY9KHN0cmVhbSxpb3YsaW92Y250LG9mZnNldCk9Pnt2YXIgcmV0PTA7Zm9yKHZhciBpPTA7aTxpb3ZjbnQ7aSsrKXt2YXIgcHRyPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtpb3Y+PjJdO3ZhciBsZW49KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2lvdis0Pj4yXTtpb3YrPTg7dmFyIGN1cnI9RlMucmVhZChzdHJlYW0sKGdyb3dNZW1WaWV3cygpLEhFQVA4KSxwdHIsbGVuLG9mZnNldCk7aWYoY3VycjwwKXJldHVybi0xO3JldCs9Y3VycjtpZihjdXJyPGxlbilicmVhaztpZih0eXBlb2Ygb2Zmc2V0IT0idW5kZWZpbmVkIil7b2Zmc2V0Kz1jdXJyfX1yZXR1cm4gcmV0fTtmdW5jdGlvbiBfZmRfcmVhZChmZCxpb3YsaW92Y250LHBudW0pe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDE1LDAsMSxmZCxpb3YsaW92Y250LHBudW0pO3RyeXt2YXIgc3RyZWFtPVNZU0NBTExTLmdldFN0cmVhbUZyb21GRChmZCk7dmFyIG51bT1kb1JlYWR2KHN0cmVhbSxpb3YsaW92Y250KTsoZ3Jvd01lbVZpZXdzKCksSEVBUFUzMilbcG51bT4+Ml09bnVtO3JldHVybiAwfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybiBlLmVycm5vfX1mdW5jdGlvbiBfZmRfc2VlayhmZCxvZmZzZXQsd2hlbmNlLG5ld09mZnNldCl7aWYoRU5WSVJPTk1FTlRfSVNfUFRIUkVBRClyZXR1cm4gcHJveHlUb01haW5UaHJlYWQoMTYsMCwxLGZkLG9mZnNldCx3aGVuY2UsbmV3T2Zmc2V0KTtvZmZzZXQ9YmlnaW50VG9JNTNDaGVja2VkKG9mZnNldCk7dHJ5e2lmKGlzTmFOKG9mZnNldCkpcmV0dXJuIDYxO3ZhciBzdHJlYW09U1lTQ0FMTFMuZ2V0U3RyZWFtRnJvbUZEKGZkKTtGUy5sbHNlZWsoc3RyZWFtLG9mZnNldCx3aGVuY2UpOyhncm93TWVtVmlld3MoKSxIRUFQNjQpW25ld09mZnNldD4+M109QmlnSW50KHN0cmVhbS5wb3NpdGlvbik7aWYoc3RyZWFtLmdldGRlbnRzJiZvZmZzZXQ9PT0wJiZ3aGVuY2U9PT0wKXN0cmVhbS5nZXRkZW50cz1udWxsO3JldHVybiAwfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybiBlLmVycm5vfX12YXIgZG9Xcml0ZXY9KHN0cmVhbSxpb3YsaW92Y250LG9mZnNldCk9Pnt2YXIgcmV0PTA7Zm9yKHZhciBpPTA7aTxpb3ZjbnQ7aSsrKXt2YXIgcHRyPShncm93TWVtVmlld3MoKSxIRUFQVTMyKVtpb3Y+PjJdO3ZhciBsZW49KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW2lvdis0Pj4yXTtpb3YrPTg7dmFyIGN1cnI9RlMud3JpdGUoc3RyZWFtLChncm93TWVtVmlld3MoKSxIRUFQOCkscHRyLGxlbixvZmZzZXQpO2lmKGN1cnI8MClyZXR1cm4tMTtyZXQrPWN1cnI7aWYoY3VycjxsZW4pe2JyZWFrfWlmKHR5cGVvZiBvZmZzZXQhPSJ1bmRlZmluZWQiKXtvZmZzZXQrPWN1cnJ9fXJldHVybiByZXR9O2Z1bmN0aW9uIF9mZF93cml0ZShmZCxpb3YsaW92Y250LHBudW0pe2lmKEVOVklST05NRU5UX0lTX1BUSFJFQUQpcmV0dXJuIHByb3h5VG9NYWluVGhyZWFkKDE3LDAsMSxmZCxpb3YsaW92Y250LHBudW0pO3RyeXt2YXIgc3RyZWFtPVNZU0NBTExTLmdldFN0cmVhbUZyb21GRChmZCk7dmFyIG51bT1kb1dyaXRldihzdHJlYW0saW92LGlvdmNudCk7KGdyb3dNZW1WaWV3cygpLEhFQVBVMzIpW3BudW0+PjJdPW51bTtyZXR1cm4gMH1jYXRjaChlKXtpZih0eXBlb2YgRlM9PSJ1bmRlZmluZWQifHwhKGUubmFtZT09PSJFcnJub0Vycm9yIikpdGhyb3cgZTtyZXR1cm4gZS5lcnJub319ZnVuY3Rpb24gX3JhbmRvbV9nZXQoYnVmZmVyLHNpemUpe3RyeXtyYW5kb21GaWxsKChncm93TWVtVmlld3MoKSxIRUFQVTgpLnN1YmFycmF5KGJ1ZmZlcixidWZmZXIrc2l6ZSkpO3JldHVybiAwfWNhdGNoKGUpe2lmKHR5cGVvZiBGUz09InVuZGVmaW5lZCJ8fCEoZS5uYW1lPT09IkVycm5vRXJyb3IiKSl0aHJvdyBlO3JldHVybiBlLmVycm5vfX12YXIgZ2V0Q0Z1bmM9aWRlbnQ9Pnt2YXIgZnVuYz1Nb2R1bGVbIl8iK2lkZW50XTthc3NlcnQoZnVuYywiQ2Fubm90IGNhbGwgdW5rbm93biBmdW5jdGlvbiAiK2lkZW50KyIsIG1ha2Ugc3VyZSBpdCBpcyBleHBvcnRlZCIpO3JldHVybiBmdW5jfTt2YXIgd3JpdGVBcnJheVRvTWVtb3J5PShhcnJheSxidWZmZXIpPT57YXNzZXJ0KGFycmF5Lmxlbmd0aD49MCwid3JpdGVBcnJheVRvTWVtb3J5IGFycmF5IG11c3QgaGF2ZSBhIGxlbmd0aCAoc2hvdWxkIGJlIGFuIGFycmF5IG9yIHR5cGVkIGFycmF5KSIpOyhncm93TWVtVmlld3MoKSxIRUFQOCkuc2V0KGFycmF5LGJ1ZmZlcil9O3ZhciBzdHJpbmdUb1VURjhPblN0YWNrPXN0cj0+e3ZhciBzaXplPWxlbmd0aEJ5dGVzVVRGOChzdHIpKzE7dmFyIHJldD1zdGFja0FsbG9jKHNpemUpO3N0cmluZ1RvVVRGOChzdHIscmV0LHNpemUpO3JldHVybiByZXR9O3ZhciBjY2FsbD0oaWRlbnQscmV0dXJuVHlwZSxhcmdUeXBlcyxhcmdzLG9wdHMpPT57dmFyIHRvQz17c3RyaW5nOnN0cj0+e3ZhciByZXQ9MDtpZihzdHIhPT1udWxsJiZzdHIhPT11bmRlZmluZWQmJnN0ciE9PTApe3JldD1zdHJpbmdUb1VURjhPblN0YWNrKHN0cil9cmV0dXJuIHJldH0sYXJyYXk6YXJyPT57dmFyIHJldD1zdGFja0FsbG9jKGFyci5sZW5ndGgpO3dyaXRlQXJyYXlUb01lbW9yeShhcnIscmV0KTtyZXR1cm4gcmV0fX07ZnVuY3Rpb24gY29udmVydFJldHVyblZhbHVlKHJldCl7aWYocmV0dXJuVHlwZT09PSJzdHJpbmciKXtyZXR1cm4gVVRGOFRvU3RyaW5nKHJldCl9aWYocmV0dXJuVHlwZT09PSJib29sZWFuIilyZXR1cm4gQm9vbGVhbihyZXQpO3JldHVybiByZXR9dmFyIGZ1bmM9Z2V0Q0Z1bmMoaWRlbnQpO3ZhciBjQXJncz1bXTt2YXIgc3RhY2s9MDthc3NlcnQocmV0dXJuVHlwZSE9PSJhcnJheSIsJ1JldHVybiB0eXBlIHNob3VsZCBub3QgYmUgImFycmF5Ii4nKTtpZihhcmdzKXtmb3IodmFyIGk9MDtpPGFyZ3MubGVuZ3RoO2krKyl7dmFyIGNvbnZlcnRlcj10b0NbYXJnVHlwZXNbaV1dO2lmKGNvbnZlcnRlcil7aWYoc3RhY2s9PT0wKXN0YWNrPXN0YWNrU2F2ZSgpO2NBcmdzW2ldPWNvbnZlcnRlcihhcmdzW2ldKX1lbHNle2NBcmdzW2ldPWFyZ3NbaV19fX12YXIgcHJldmlvdXNBc3luYz1Bc3luY2lmeS5jdXJyRGF0YTt2YXIgcmV0PWZ1bmMoLi4uY0FyZ3MpO2Z1bmN0aW9uIG9uRG9uZShyZXQpe3J1bnRpbWVLZWVwYWxpdmVQb3AoKTtpZihzdGFjayE9PTApc3RhY2tSZXN0b3JlKHN0YWNrKTtyZXR1cm4gY29udmVydFJldHVyblZhbHVlKHJldCl9dmFyIGFzeW5jTW9kZT1vcHRzPy5hc3luYztydW50aW1lS2VlcGFsaXZlUHVzaCgpO2lmKEFzeW5jaWZ5LmN1cnJEYXRhIT1wcmV2aW91c0FzeW5jKXthc3NlcnQoIShwcmV2aW91c0FzeW5jJiZBc3luY2lmeS5jdXJyRGF0YSksIldlIGNhbm5vdCBzdGFydCBhbiBhc3luYyBvcGVyYXRpb24gd2hlbiBvbmUgaXMgYWxyZWFkeSBmbGlnaHQiKTthc3NlcnQoIShwcmV2aW91c0FzeW5jJiYhQXN5bmNpZnkuY3VyckRhdGEpLCJXZSBjYW5ub3Qgc3RvcCBhbiBhc3luYyBvcGVyYXRpb24gaW4gZmxpZ2h0Iik7YXNzZXJ0KGFzeW5jTW9kZSwiVGhlIGNhbGwgdG8gIitpZGVudCsiIGlzIHJ1bm5pbmcgYXN5bmNocm9ub3VzbHkuIElmIHRoaXMgd2FzIGludGVuZGVkLCBhZGQgdGhlIGFzeW5jIG9wdGlvbiB0byB0aGUgY2NhbGwvY3dyYXAgY2FsbC4iKTtyZXR1cm4gQXN5bmNpZnkud2hlbkRvbmUoKS50aGVuKG9uRG9uZSl9cmV0PW9uRG9uZShyZXQpO2lmKGFzeW5jTW9kZSlyZXR1cm4gUHJvbWlzZS5yZXNvbHZlKHJldCk7cmV0dXJuIHJldH07dmFyIGN3cmFwPShpZGVudCxyZXR1cm5UeXBlLGFyZ1R5cGVzLG9wdHMpPT4oLi4uYXJncyk9PmNjYWxsKGlkZW50LHJldHVyblR5cGUsYXJnVHlwZXMsYXJncyxvcHRzKTt2YXIgYWxsb2NhdGVVVEY4PSguLi5hcmdzKT0+c3RyaW5nVG9OZXdVVEY4KC4uLmFyZ3MpO1BUaHJlYWQuaW5pdCgpO0ZTLmNyZWF0ZVByZWxvYWRlZEZpbGU9RlNfY3JlYXRlUHJlbG9hZGVkRmlsZTtGUy5wcmVsb2FkRmlsZT1GU19wcmVsb2FkRmlsZTtGUy5zdGF0aWNJbml0KCk7YXNzZXJ0KGVtdmFsX2hhbmRsZXMubGVuZ3RoPT09NSoyKTt7aW5pdE1lbW9yeSgpO2lmKE1vZHVsZVsibm9FeGl0UnVudGltZSJdKW5vRXhpdFJ1bnRpbWU9TW9kdWxlWyJub0V4aXRSdW50aW1lIl07aWYoTW9kdWxlWyJwcmVsb2FkUGx1Z2lucyJdKXByZWxvYWRQbHVnaW5zPU1vZHVsZVsicHJlbG9hZFBsdWdpbnMiXTtpZihNb2R1bGVbInByaW50Il0pb3V0PU1vZHVsZVsicHJpbnQiXTtpZihNb2R1bGVbInByaW50RXJyIl0pZXJyPU1vZHVsZVsicHJpbnRFcnIiXTtpZihNb2R1bGVbIndhc21CaW5hcnkiXSl3YXNtQmluYXJ5PU1vZHVsZVsid2FzbUJpbmFyeSJdO2NoZWNrSW5jb21pbmdNb2R1bGVBUEkoKTtpZihNb2R1bGVbImFyZ3VtZW50cyJdKWFyZ3VtZW50c189TW9kdWxlWyJhcmd1bWVudHMiXTtpZihNb2R1bGVbInRoaXNQcm9ncmFtIl0pdGhpc1Byb2dyYW09TW9kdWxlWyJ0aGlzUHJvZ3JhbSJdO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJtZW1vcnlJbml0aWFsaXplclByZWZpeFVSTCJdPT0idW5kZWZpbmVkIiwiTW9kdWxlLm1lbW9yeUluaXRpYWxpemVyUHJlZml4VVJMIG9wdGlvbiB3YXMgcmVtb3ZlZCwgdXNlIE1vZHVsZS5sb2NhdGVGaWxlIGluc3RlYWQiKTthc3NlcnQodHlwZW9mIE1vZHVsZVsicHRocmVhZE1haW5QcmVmaXhVUkwiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5wdGhyZWFkTWFpblByZWZpeFVSTCBvcHRpb24gd2FzIHJlbW92ZWQsIHVzZSBNb2R1bGUubG9jYXRlRmlsZSBpbnN0ZWFkIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbImNkSW5pdGlhbGl6ZXJQcmVmaXhVUkwiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5jZEluaXRpYWxpemVyUHJlZml4VVJMIG9wdGlvbiB3YXMgcmVtb3ZlZCwgdXNlIE1vZHVsZS5sb2NhdGVGaWxlIGluc3RlYWQiKTthc3NlcnQodHlwZW9mIE1vZHVsZVsiZmlsZVBhY2thZ2VQcmVmaXhVUkwiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5maWxlUGFja2FnZVByZWZpeFVSTCBvcHRpb24gd2FzIHJlbW92ZWQsIHVzZSBNb2R1bGUubG9jYXRlRmlsZSBpbnN0ZWFkIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbInJlYWQiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5yZWFkIG9wdGlvbiB3YXMgcmVtb3ZlZCIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJyZWFkQXN5bmMiXT09InVuZGVmaW5lZCIsIk1vZHVsZS5yZWFkQXN5bmMgb3B0aW9uIHdhcyByZW1vdmVkIChtb2RpZnkgcmVhZEFzeW5jIGluIEpTKSIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJyZWFkQmluYXJ5Il09PSJ1bmRlZmluZWQiLCJNb2R1bGUucmVhZEJpbmFyeSBvcHRpb24gd2FzIHJlbW92ZWQgKG1vZGlmeSByZWFkQmluYXJ5IGluIEpTKSIpO2Fzc2VydCh0eXBlb2YgTW9kdWxlWyJzZXRXaW5kb3dUaXRsZSJdPT0idW5kZWZpbmVkIiwiTW9kdWxlLnNldFdpbmRvd1RpdGxlIG9wdGlvbiB3YXMgcmVtb3ZlZCAobW9kaWZ5IGVtc2NyaXB0ZW5fc2V0X3dpbmRvd190aXRsZSBpbiBKUykiKTthc3NlcnQodHlwZW9mIE1vZHVsZVsiVE9UQUxfTUVNT1JZIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUuVE9UQUxfTUVNT1JZIGhhcyBiZWVuIHJlbmFtZWQgTW9kdWxlLklOSVRJQUxfTUVNT1JZIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbIkVOVklST05NRU5UIl09PSJ1bmRlZmluZWQiLCJNb2R1bGUuRU5WSVJPTk1FTlQgaGFzIGJlZW4gZGVwcmVjYXRlZC4gVG8gZm9yY2UgdGhlIGVudmlyb25tZW50LCB1c2UgdGhlIEVOVklST05NRU5UIGNvbXBpbGUtdGltZSBvcHRpb24gKGZvciBleGFtcGxlLCAtc0VOVklST05NRU5UPXdlYiBvciAtc0VOVklST05NRU5UPW5vZGUpIik7YXNzZXJ0KHR5cGVvZiBNb2R1bGVbIlNUQUNLX1NJWkUiXT09InVuZGVmaW5lZCIsIlNUQUNLX1NJWkUgY2FuIG5vIGxvbmdlciBiZSBzZXQgYXQgcnVudGltZS4gIFVzZSAtc1NUQUNLX1NJWkUgYXQgbGluayB0aW1lIik7aWYoTW9kdWxlWyJwcmVJbml0Il0pe2lmKHR5cGVvZiBNb2R1bGVbInByZUluaXQiXT09ImZ1bmN0aW9uIilNb2R1bGVbInByZUluaXQiXT1bTW9kdWxlWyJwcmVJbml0Il1dO3doaWxlKE1vZHVsZVsicHJlSW5pdCJdLmxlbmd0aD4wKXtNb2R1bGVbInByZUluaXQiXS5zaGlmdCgpKCl9fWNvbnN1bWVkTW9kdWxlUHJvcCgicHJlSW5pdCIpfU1vZHVsZVsiY2NhbGwiXT1jY2FsbDtNb2R1bGVbImN3cmFwIl09Y3dyYXA7TW9kdWxlWyJVVEY4VG9TdHJpbmciXT1VVEY4VG9TdHJpbmc7TW9kdWxlWyJzdHJpbmdUb1VURjgiXT1zdHJpbmdUb1VURjg7TW9kdWxlWyJhbGxvY2F0ZVVURjgiXT1hbGxvY2F0ZVVURjg7dmFyIG1pc3NpbmdMaWJyYXJ5U3ltYm9scz1bIndyaXRlSTUzVG9JNjQiLCJ3cml0ZUk1M1RvSTY0Q2xhbXBlZCIsIndyaXRlSTUzVG9JNjRTaWduYWxpbmciLCJ3cml0ZUk1M1RvVTY0Q2xhbXBlZCIsIndyaXRlSTUzVG9VNjRTaWduYWxpbmciLCJyZWFkSTUzRnJvbUk2NCIsInJlYWRJNTNGcm9tVTY0IiwiY29udmVydEkzMlBhaXJUb0k1MyIsImNvbnZlcnRJMzJQYWlyVG9JNTNDaGVja2VkIiwiY29udmVydFUzMlBhaXJUb0k1MyIsImdldFRlbXBSZXQwIiwic2V0VGVtcFJldDAiLCJ3aXRoU3RhY2tTYXZlIiwiaW5ldFB0b240IiwiaW5ldE50b3A0IiwiaW5ldFB0b242IiwiaW5ldE50b3A2IiwicmVhZFNvY2thZGRyIiwid3JpdGVTb2NrYWRkciIsInJ1bk1haW5UaHJlYWRFbUFzbSIsImpzdG9pX3EiLCJhdXRvUmVzdW1lQXVkaW9Db250ZXh0IiwiYXNtanNNYW5nbGUiLCJIYW5kbGVBbGxvY2F0b3IiLCJhZGRPbkluaXQiLCJhZGRPblBvc3RDdG9yIiwiYWRkT25QcmVNYWluIiwiYWRkT25FeGl0IiwiU1RBQ0tfU0laRSIsIlNUQUNLX0FMSUdOIiwiUE9JTlRFUl9TSVpFIiwiQVNTRVJUSU9OUyIsImNvbnZlcnRKc0Z1bmN0aW9uVG9XYXNtIiwiZ2V0RW1wdHlUYWJsZVNsb3QiLCJ1cGRhdGVUYWJsZU1hcCIsImdldEZ1bmN0aW9uQWRkcmVzcyIsImFkZEZ1bmN0aW9uIiwicmVtb3ZlRnVuY3Rpb24iLCJpbnRBcnJheVRvU3RyaW5nIiwic3RyaW5nVG9Bc2NpaSIsInJlZ2lzdGVyS2V5RXZlbnRDYWxsYmFjayIsIm1heWJlQ1N0cmluZ1RvSnNTdHJpbmciLCJmaW5kRXZlbnRUYXJnZXQiLCJnZXRCb3VuZGluZ0NsaWVudFJlY3QiLCJmaWxsTW91c2VFdmVudERhdGEiLCJyZWdpc3Rlck1vdXNlRXZlbnRDYWxsYmFjayIsInJlZ2lzdGVyV2hlZWxFdmVudENhbGxiYWNrIiwicmVnaXN0ZXJVaUV2ZW50Q2FsbGJhY2siLCJyZWdpc3RlckZvY3VzRXZlbnRDYWxsYmFjayIsImZpbGxEZXZpY2VPcmllbnRhdGlvbkV2ZW50RGF0YSIsInJlZ2lzdGVyRGV2aWNlT3JpZW50YXRpb25FdmVudENhbGxiYWNrIiwiZmlsbERldmljZU1vdGlvbkV2ZW50RGF0YSIsInJlZ2lzdGVyRGV2aWNlTW90aW9uRXZlbnRDYWxsYmFjayIsInNjcmVlbk9yaWVudGF0aW9uIiwiZmlsbE9yaWVudGF0aW9uQ2hhbmdlRXZlbnREYXRhIiwicmVnaXN0ZXJPcmllbnRhdGlvbkNoYW5nZUV2ZW50Q2FsbGJhY2siLCJmaWxsRnVsbHNjcmVlbkNoYW5nZUV2ZW50RGF0YSIsInJlZ2lzdGVyRnVsbHNjcmVlbkNoYW5nZUV2ZW50Q2FsbGJhY2siLCJKU0V2ZW50c19yZXF1ZXN0RnVsbHNjcmVlbiIsIkpTRXZlbnRzX3Jlc2l6ZUNhbnZhc0ZvckZ1bGxzY3JlZW4iLCJyZWdpc3RlclJlc3RvcmVPbGRTdHlsZSIsImhpZGVFdmVyeXRoaW5nRXhjZXB0R2l2ZW5FbGVtZW50IiwicmVzdG9yZUhpZGRlbkVsZW1lbnRzIiwic2V0TGV0dGVyYm94Iiwic29mdEZ1bGxzY3JlZW5SZXNpemVXZWJHTFJlbmRlclRhcmdldCIsImRvUmVxdWVzdEZ1bGxzY3JlZW4iLCJmaWxsUG9pbnRlcmxvY2tDaGFuZ2VFdmVudERhdGEiLCJyZWdpc3RlclBvaW50ZXJsb2NrQ2hhbmdlRXZlbnRDYWxsYmFjayIsInJlZ2lzdGVyUG9pbnRlcmxvY2tFcnJvckV2ZW50Q2FsbGJhY2siLCJyZXF1ZXN0UG9pbnRlckxvY2siLCJmaWxsVmlzaWJpbGl0eUNoYW5nZUV2ZW50RGF0YSIsInJlZ2lzdGVyVmlzaWJpbGl0eUNoYW5nZUV2ZW50Q2FsbGJhY2siLCJyZWdpc3RlclRvdWNoRXZlbnRDYWxsYmFjayIsImZpbGxHYW1lcGFkRXZlbnREYXRhIiwicmVnaXN0ZXJHYW1lcGFkRXZlbnRDYWxsYmFjayIsInJlZ2lzdGVyQmVmb3JlVW5sb2FkRXZlbnRDYWxsYmFjayIsImZpbGxCYXR0ZXJ5RXZlbnREYXRhIiwicmVnaXN0ZXJCYXR0ZXJ5RXZlbnRDYWxsYmFjayIsInNldENhbnZhc0VsZW1lbnRTaXplQ2FsbGluZ1RocmVhZCIsInNldENhbnZhc0VsZW1lbnRTaXplTWFpblRocmVhZCIsInNldENhbnZhc0VsZW1lbnRTaXplIiwiZ2V0Q2FudmFzU2l6ZUNhbGxpbmdUaHJlYWQiLCJnZXRDYW52YXNTaXplTWFpblRocmVhZCIsImdldENhbnZhc0VsZW1lbnRTaXplIiwiZ2V0Q2FsbHN0YWNrIiwiY29udmVydFBDdG9Tb3VyY2VMb2NhdGlvbiIsIndhc2lSaWdodHNUb011c2xPRmxhZ3MiLCJ3YXNpT0ZsYWdzVG9NdXNsT0ZsYWdzIiwic2FmZVNldFRpbWVvdXQiLCJzZXRJbW1lZGlhdGVXcmFwcGVkIiwic2FmZVJlcXVlc3RBbmltYXRpb25GcmFtZSIsImNsZWFySW1tZWRpYXRlV3JhcHBlZCIsInJlZ2lzdGVyUG9zdE1haW5Mb29wIiwicmVnaXN0ZXJQcmVNYWluTG9vcCIsImdldFByb21pc2UiLCJtYWtlUHJvbWlzZSIsImlkc1RvUHJvbWlzZXMiLCJtYWtlUHJvbWlzZUNhbGxiYWNrIiwiZmluZE1hdGNoaW5nQ2F0Y2giLCJCcm93c2VyX2FzeW5jUHJlcGFyZURhdGFDb3VudGVyIiwiaXNMZWFwWWVhciIsInlkYXlGcm9tRGF0ZSIsImFycmF5U3VtIiwiYWRkRGF5cyIsImdldFNvY2tldEZyb21GRCIsImdldFNvY2tldEFkZHJlc3MiLCJGU19ta2RpclRyZWUiLCJfc2V0TmV0d29ya0NhbGxiYWNrIiwiaGVhcE9iamVjdEZvcldlYkdMVHlwZSIsInRvVHlwZWRBcnJheUluZGV4Iiwid2ViZ2xfZW5hYmxlX0FOR0xFX2luc3RhbmNlZF9hcnJheXMiLCJ3ZWJnbF9lbmFibGVfT0VTX3ZlcnRleF9hcnJheV9vYmplY3QiLCJ3ZWJnbF9lbmFibGVfV0VCR0xfZHJhd19idWZmZXJzIiwid2ViZ2xfZW5hYmxlX1dFQkdMX211bHRpX2RyYXciLCJ3ZWJnbF9lbmFibGVfRVhUX3BvbHlnb25fb2Zmc2V0X2NsYW1wIiwid2ViZ2xfZW5hYmxlX0VYVF9jbGlwX2NvbnRyb2wiLCJ3ZWJnbF9lbmFibGVfV0VCR0xfcG9seWdvbl9tb2RlIiwiZW1zY3JpcHRlbldlYkdMR2V0IiwiY29tcHV0ZVVucGFja0FsaWduZWRJbWFnZVNpemUiLCJjb2xvckNoYW5uZWxzSW5HbFRleHR1cmVGb3JtYXQiLCJlbXNjcmlwdGVuV2ViR0xHZXRUZXhQaXhlbERhdGEiLCJlbXNjcmlwdGVuV2ViR0xHZXRVbmlmb3JtIiwid2ViZ2xHZXRVbmlmb3JtTG9jYXRpb24iLCJ3ZWJnbFByZXBhcmVVbmlmb3JtTG9jYXRpb25zQmVmb3JlRmlyc3RVc2UiLCJ3ZWJnbEdldExlZnRCcmFjZVBvcyIsImVtc2NyaXB0ZW5XZWJHTEdldFZlcnRleEF0dHJpYiIsIl9fZ2xHZXRBY3RpdmVBdHRyaWJPclVuaWZvcm0iLCJ3cml0ZUdMQXJyYXkiLCJlbXNjcmlwdGVuX3dlYmdsX2Rlc3Ryb3lfY29udGV4dF9iZWZvcmVfb25fY2FsbGluZ190aHJlYWQiLCJyZWdpc3RlcldlYkdsRXZlbnRDYWxsYmFjayIsIkFMTE9DX05PUk1BTCIsIkFMTE9DX1NUQUNLIiwiYWxsb2NhdGUiLCJ3cml0ZVN0cmluZ1RvTWVtb3J5Iiwid3JpdGVBc2NpaVRvTWVtb3J5IiwiYWxsb2NhdGVVVEY4T25TdGFjayIsImRlbWFuZ2xlIiwic3RhY2tUcmFjZSIsImdldE5hdGl2ZVR5cGVTaXplIiwiZ2V0RnVuY3Rpb25BcmdzTmFtZSIsInJlcXVpcmVSZWdpc3RlcmVkVHlwZSIsImNyZWF0ZUpzSW52b2tlclNpZ25hdHVyZSIsIlB1cmVWaXJ0dWFsRXJyb3IiLCJnZXRCYXNlc3RQb2ludGVyIiwicmVnaXN0ZXJJbmhlcml0ZWRJbnN0YW5jZSIsInVucmVnaXN0ZXJJbmhlcml0ZWRJbnN0YW5jZSIsImdldEluaGVyaXRlZEluc3RhbmNlIiwiZ2V0SW5oZXJpdGVkSW5zdGFuY2VDb3VudCIsImdldExpdmVJbmhlcml0ZWRJbnN0YW5jZXMiLCJlbnVtUmVhZFZhbHVlRnJvbVBvaW50ZXIiLCJnZW5lcmljUG9pbnRlclRvV2lyZVR5cGUiLCJjb25zdE5vU21hcnRQdHJSYXdQb2ludGVyVG9XaXJlVHlwZSIsIm5vbkNvbnN0Tm9TbWFydFB0clJhd1BvaW50ZXJUb1dpcmVUeXBlIiwiaW5pdF9SZWdpc3RlcmVkUG9pbnRlciIsIlJlZ2lzdGVyZWRQb2ludGVyIiwiUmVnaXN0ZXJlZFBvaW50ZXJfZnJvbVdpcmVUeXBlIiwicnVuRGVzdHJ1Y3RvciIsInJlbGVhc2VDbGFzc0hhbmRsZSIsImRldGFjaEZpbmFsaXplciIsImF0dGFjaEZpbmFsaXplciIsIm1ha2VDbGFzc0hhbmRsZSIsImluaXRfQ2xhc3NIYW5kbGUiLCJDbGFzc0hhbmRsZSIsInRocm93SW5zdGFuY2VBbHJlYWR5RGVsZXRlZCIsImZsdXNoUGVuZGluZ0RlbGV0ZXMiLCJzZXREZWxheUZ1bmN0aW9uIiwiUmVnaXN0ZXJlZENsYXNzIiwic2hhbGxvd0NvcHlJbnRlcm5hbFBvaW50ZXIiLCJkb3duY2FzdFBvaW50ZXIiLCJ1cGNhc3RQb2ludGVyIiwidmFsaWRhdGVUaGlzIiwiY2hhcl8wIiwiY2hhcl85IiwibWFrZUxlZ2FsRnVuY3Rpb25OYW1lIiwiY291bnRfZW12YWxfaGFuZGxlcyIsImdldFN0cmluZ09yU3ltYm9sIiwiZW12YWxfcmV0dXJuVmFsdWUiLCJlbXZhbF9sb29rdXBUeXBlcyIsImVtdmFsX2FkZE1ldGhvZENhbGxlciJdO21pc3NpbmdMaWJyYXJ5U3ltYm9scy5mb3JFYWNoKG1pc3NpbmdMaWJyYXJ5U3ltYm9sKTt2YXIgdW5leHBvcnRlZFN5bWJvbHM9WyJydW4iLCJvdXQiLCJlcnIiLCJjYWxsTWFpbiIsImFib3J0Iiwid2FzbUV4cG9ydHMiLCJIRUFQRjMyIiwiSEVBUEY2NCIsIkhFQVA4IiwiSEVBUDE2IiwiSEVBUFUxNiIsIkhFQVAzMiIsIkhFQVBVMzIiLCJIRUFQNjQiLCJIRUFQVTY0Iiwid3JpdGVTdGFja0Nvb2tpZSIsImNoZWNrU3RhY2tDb29raWUiLCJJTlQ1M19NQVgiLCJJTlQ1M19NSU4iLCJiaWdpbnRUb0k1M0NoZWNrZWQiLCJzdGFja1NhdmUiLCJzdGFja1Jlc3RvcmUiLCJzdGFja0FsbG9jIiwiY3JlYXRlTmFtZWRGdW5jdGlvbiIsInB0clRvU3RyaW5nIiwiemVyb01lbW9yeSIsImV4aXRKUyIsImdldEhlYXBNYXgiLCJncm93TWVtb3J5IiwiRU5WIiwiRVJSTk9fQ09ERVMiLCJzdHJFcnJvciIsIkROUyIsIlByb3RvY29scyIsIlNvY2tldHMiLCJ0aW1lcnMiLCJ3YXJuT25jZSIsInJlYWRFbUFzbUFyZ3NBcnJheSIsInJlYWRFbUFzbUFyZ3MiLCJydW5FbUFzbUZ1bmN0aW9uIiwiZ2V0RXhlY3V0YWJsZU5hbWUiLCJkeW5DYWxsTGVnYWN5IiwiZ2V0RHluQ2FsbGVyIiwiZHluQ2FsbCIsImhhbmRsZUV4Y2VwdGlvbiIsImtlZXBSdW50aW1lQWxpdmUiLCJydW50aW1lS2VlcGFsaXZlUHVzaCIsInJ1bnRpbWVLZWVwYWxpdmVQb3AiLCJjYWxsVXNlckNhbGxiYWNrIiwibWF5YmVFeGl0IiwiYXN5bmNMb2FkIiwiYWxpZ25NZW1vcnkiLCJtbWFwQWxsb2MiLCJ3YXNtVGFibGUiLCJ3YXNtTWVtb3J5IiwiZ2V0VW5pcXVlUnVuRGVwZW5kZW5jeSIsIm5vRXhpdFJ1bnRpbWUiLCJhZGRSdW5EZXBlbmRlbmN5IiwicmVtb3ZlUnVuRGVwZW5kZW5jeSIsImFkZE9uUHJlUnVuIiwiYWRkT25Qb3N0UnVuIiwiZnJlZVRhYmxlSW5kZXhlcyIsImZ1bmN0aW9uc0luVGFibGVNYXAiLCJzZXRWYWx1ZSIsImdldFZhbHVlIiwiUEFUSCIsIlBBVEhfRlMiLCJVVEY4RGVjb2RlciIsIlVURjhBcnJheVRvU3RyaW5nIiwic3RyaW5nVG9VVEY4QXJyYXkiLCJsZW5ndGhCeXRlc1VURjgiLCJpbnRBcnJheUZyb21TdHJpbmciLCJBc2NpaVRvU3RyaW5nIiwiVVRGMTZEZWNvZGVyIiwiVVRGMTZUb1N0cmluZyIsInN0cmluZ1RvVVRGMTYiLCJsZW5ndGhCeXRlc1VURjE2IiwiVVRGMzJUb1N0cmluZyIsInN0cmluZ1RvVVRGMzIiLCJsZW5ndGhCeXRlc1VURjMyIiwic3RyaW5nVG9OZXdVVEY4Iiwic3RyaW5nVG9VVEY4T25TdGFjayIsIndyaXRlQXJyYXlUb01lbW9yeSIsIkpTRXZlbnRzIiwic3BlY2lhbEhUTUxUYXJnZXRzIiwiZmluZENhbnZhc0V2ZW50VGFyZ2V0IiwiY3VycmVudEZ1bGxzY3JlZW5TdHJhdGVneSIsInJlc3RvcmVPbGRXaW5kb3dlZFN0eWxlIiwianNTdGFja1RyYWNlIiwiVU5XSU5EX0NBQ0hFIiwiRXhpdFN0YXR1cyIsImdldEVudlN0cmluZ3MiLCJjaGVja1dhc2lDbG9jayIsImRvUmVhZHYiLCJkb1dyaXRldiIsImluaXRSYW5kb21GaWxsIiwicmFuZG9tRmlsbCIsImVtU2V0SW1tZWRpYXRlIiwiZW1DbGVhckltbWVkaWF0ZV9kZXBzIiwiZW1DbGVhckltbWVkaWF0ZSIsInByb21pc2VNYXAiLCJ1bmNhdWdodEV4Y2VwdGlvbkNvdW50IiwiZXhjZXB0aW9uTGFzdCIsImV4Y2VwdGlvbkNhdWdodCIsIkV4Y2VwdGlvbkluZm8iLCJCcm93c2VyIiwicmVxdWVzdEZ1bGxzY3JlZW4iLCJyZXF1ZXN0RnVsbFNjcmVlbiIsInNldENhbnZhc1NpemUiLCJnZXRVc2VyTWVkaWEiLCJjcmVhdGVDb250ZXh0IiwiZ2V0UHJlbG9hZGVkSW1hZ2VEYXRhX19kYXRhIiwid2dldCIsIk1PTlRIX0RBWVNfUkVHVUxBUiIsIk1PTlRIX0RBWVNfTEVBUCIsIk1PTlRIX0RBWVNfUkVHVUxBUl9DVU1VTEFUSVZFIiwiTU9OVEhfREFZU19MRUFQX0NVTVVMQVRJVkUiLCJTWVNDQUxMUyIsInByZWxvYWRQbHVnaW5zIiwiRlNfY3JlYXRlUHJlbG9hZGVkRmlsZSIsIkZTX3ByZWxvYWRGaWxlIiwiRlNfbW9kZVN0cmluZ1RvRmxhZ3MiLCJGU19nZXRNb2RlIiwiRlNfc3RkaW5fZ2V0Q2hhcl9idWZmZXIiLCJGU19zdGRpbl9nZXRDaGFyIiwiRlNfdW5saW5rIiwiRlNfY3JlYXRlUGF0aCIsIkZTX2NyZWF0ZURldmljZSIsIkZTX3JlYWRGaWxlIiwiRlMiLCJGU19yb290IiwiRlNfbW91bnRzIiwiRlNfZGV2aWNlcyIsIkZTX3N0cmVhbXMiLCJGU19uZXh0SW5vZGUiLCJGU19uYW1lVGFibGUiLCJGU19jdXJyZW50UGF0aCIsIkZTX2luaXRpYWxpemVkIiwiRlNfaWdub3JlUGVybWlzc2lvbnMiLCJGU19maWxlc3lzdGVtcyIsIkZTX3N5bmNGU1JlcXVlc3RzIiwiRlNfcmVhZEZpbGVzIiwiRlNfbG9va3VwUGF0aCIsIkZTX2dldFBhdGgiLCJGU19oYXNoTmFtZSIsIkZTX2hhc2hBZGROb2RlIiwiRlNfaGFzaFJlbW92ZU5vZGUiLCJGU19sb29rdXBOb2RlIiwiRlNfY3JlYXRlTm9kZSIsIkZTX2Rlc3Ryb3lOb2RlIiwiRlNfaXNSb290IiwiRlNfaXNNb3VudHBvaW50IiwiRlNfaXNGaWxlIiwiRlNfaXNEaXIiLCJGU19pc0xpbmsiLCJGU19pc0NocmRldiIsIkZTX2lzQmxrZGV2IiwiRlNfaXNGSUZPIiwiRlNfaXNTb2NrZXQiLCJGU19mbGFnc1RvUGVybWlzc2lvblN0cmluZyIsIkZTX25vZGVQZXJtaXNzaW9ucyIsIkZTX21heUxvb2t1cCIsIkZTX21heUNyZWF0ZSIsIkZTX21heURlbGV0ZSIsIkZTX21heU9wZW4iLCJGU19jaGVja09wRXhpc3RzIiwiRlNfbmV4dGZkIiwiRlNfZ2V0U3RyZWFtQ2hlY2tlZCIsIkZTX2dldFN0cmVhbSIsIkZTX2NyZWF0ZVN0cmVhbSIsIkZTX2Nsb3NlU3RyZWFtIiwiRlNfZHVwU3RyZWFtIiwiRlNfZG9TZXRBdHRyIiwiRlNfY2hyZGV2X3N0cmVhbV9vcHMiLCJGU19tYWpvciIsIkZTX21pbm9yIiwiRlNfbWFrZWRldiIsIkZTX3JlZ2lzdGVyRGV2aWNlIiwiRlNfZ2V0RGV2aWNlIiwiRlNfZ2V0TW91bnRzIiwiRlNfc3luY2ZzIiwiRlNfbW91bnQiLCJGU191bm1vdW50IiwiRlNfbG9va3VwIiwiRlNfbWtub2QiLCJGU19zdGF0ZnMiLCJGU19zdGF0ZnNTdHJlYW0iLCJGU19zdGF0ZnNOb2RlIiwiRlNfY3JlYXRlIiwiRlNfbWtkaXIiLCJGU19ta2RldiIsIkZTX3N5bWxpbmsiLCJGU19yZW5hbWUiLCJGU19ybWRpciIsIkZTX3JlYWRkaXIiLCJGU19yZWFkbGluayIsIkZTX3N0YXQiLCJGU19mc3RhdCIsIkZTX2xzdGF0IiwiRlNfZG9DaG1vZCIsIkZTX2NobW9kIiwiRlNfbGNobW9kIiwiRlNfZmNobW9kIiwiRlNfZG9DaG93biIsIkZTX2Nob3duIiwiRlNfbGNob3duIiwiRlNfZmNob3duIiwiRlNfZG9UcnVuY2F0ZSIsIkZTX3RydW5jYXRlIiwiRlNfZnRydW5jYXRlIiwiRlNfdXRpbWUiLCJGU19vcGVuIiwiRlNfY2xvc2UiLCJGU19pc0Nsb3NlZCIsIkZTX2xsc2VlayIsIkZTX3JlYWQiLCJGU193cml0ZSIsIkZTX21tYXAiLCJGU19tc3luYyIsIkZTX2lvY3RsIiwiRlNfd3JpdGVGaWxlIiwiRlNfY3dkIiwiRlNfY2hkaXIiLCJGU19jcmVhdGVEZWZhdWx0RGlyZWN0b3JpZXMiLCJGU19jcmVhdGVEZWZhdWx0RGV2aWNlcyIsIkZTX2NyZWF0ZVNwZWNpYWxEaXJlY3RvcmllcyIsIkZTX2NyZWF0ZVN0YW5kYXJkU3RyZWFtcyIsIkZTX3N0YXRpY0luaXQiLCJGU19pbml0IiwiRlNfcXVpdCIsIkZTX2ZpbmRPYmplY3QiLCJGU19hbmFseXplUGF0aCIsIkZTX2NyZWF0ZUZpbGUiLCJGU19jcmVhdGVEYXRhRmlsZSIsIkZTX2ZvcmNlTG9hZEZpbGUiLCJGU19jcmVhdGVMYXp5RmlsZSIsIkZTX2Fic29sdXRlUGF0aCIsIkZTX2NyZWF0ZUZvbGRlciIsIkZTX2NyZWF0ZUxpbmsiLCJGU19qb2luUGF0aCIsIkZTX21tYXBBbGxvYyIsIkZTX3N0YW5kYXJkaXplUGF0aCIsIk1FTUZTIiwiVFRZIiwiUElQRUZTIiwiU09DS0ZTIiwidGVtcEZpeGVkTGVuZ3RoQXJyYXkiLCJtaW5pVGVtcFdlYkdMRmxvYXRCdWZmZXJzIiwibWluaVRlbXBXZWJHTEludEJ1ZmZlcnMiLCJHTCIsIkFMIiwiR0xVVCIsIkVHTCIsIkdMRVciLCJJREJTdG9yZSIsInJ1bkFuZEFib3J0SWZFcnJvciIsIkFzeW5jaWZ5IiwiRmliZXJzIiwiU0RMIiwiU0RMX2dmeCIsInByaW50IiwicHJpbnRFcnIiLCJqc3RvaV9zIiwiUFRocmVhZCIsInRlcm1pbmF0ZVdvcmtlciIsImNsZWFudXBUaHJlYWQiLCJyZWdpc3RlclRMU0luaXQiLCJzcGF3blRocmVhZCIsImV4aXRPbk1haW5UaHJlYWQiLCJwcm94eVRvTWFpblRocmVhZCIsInByb3hpZWRKU0NhbGxBcmdzIiwiaW52b2tlRW50cnlQb2ludCIsImNoZWNrTWFpbGJveCIsIkludGVybmFsRXJyb3IiLCJCaW5kaW5nRXJyb3IiLCJ0aHJvd0ludGVybmFsRXJyb3IiLCJ0aHJvd0JpbmRpbmdFcnJvciIsInJlZ2lzdGVyZWRUeXBlcyIsImF3YWl0aW5nRGVwZW5kZW5jaWVzIiwidHlwZURlcGVuZGVuY2llcyIsInR1cGxlUmVnaXN0cmF0aW9ucyIsInN0cnVjdFJlZ2lzdHJhdGlvbnMiLCJzaGFyZWRSZWdpc3RlclR5cGUiLCJ3aGVuRGVwZW5kZW50VHlwZXNBcmVSZXNvbHZlZCIsImdldFR5cGVOYW1lIiwiZ2V0RnVuY3Rpb25OYW1lIiwiaGVhcDMyVmVjdG9yVG9BcnJheSIsInVzZXNEZXN0cnVjdG9yU3RhY2siLCJjaGVja0FyZ0NvdW50IiwiZ2V0UmVxdWlyZWRBcmdDb3VudCIsImNyZWF0ZUpzSW52b2tlciIsIlVuYm91bmRUeXBlRXJyb3IiLCJFbVZhbFR5cGUiLCJFbVZhbE9wdGlvbmFsVHlwZSIsInRocm93VW5ib3VuZFR5cGVFcnJvciIsImVuc3VyZU92ZXJsb2FkVGFibGUiLCJleHBvc2VQdWJsaWNTeW1ib2wiLCJyZXBsYWNlUHVibGljU3ltYm9sIiwiZW1iaW5kUmVwciIsInJlZ2lzdGVyZWRJbnN0YW5jZXMiLCJyZWdpc3RlcmVkUG9pbnRlcnMiLCJyZWdpc3RlclR5cGUiLCJpbnRlZ2VyUmVhZFZhbHVlRnJvbVBvaW50ZXIiLCJmbG9hdFJlYWRWYWx1ZUZyb21Qb2ludGVyIiwiYXNzZXJ0SW50ZWdlclJhbmdlIiwicmVhZFBvaW50ZXIiLCJydW5EZXN0cnVjdG9ycyIsImNyYWZ0SW52b2tlckZ1bmN0aW9uIiwiZW1iaW5kX19yZXF1aXJlRnVuY3Rpb24iLCJmaW5hbGl6YXRpb25SZWdpc3RyeSIsImRldGFjaEZpbmFsaXplcl9kZXBzIiwiZGVsZXRpb25RdWV1ZSIsImRlbGF5RnVuY3Rpb24iLCJlbXZhbF9mcmVlbGlzdCIsImVtdmFsX2hhbmRsZXMiLCJlbXZhbF9zeW1ib2xzIiwiRW12YWwiLCJlbXZhbF9tZXRob2RDYWxsZXJzIl07dW5leHBvcnRlZFN5bWJvbHMuZm9yRWFjaCh1bmV4cG9ydGVkUnVudGltZVN5bWJvbCk7dmFyIHByb3hpZWRGdW5jdGlvblRhYmxlPVtfcHJvY19leGl0LGV4aXRPbk1haW5UaHJlYWQscHRocmVhZENyZWF0ZVByb3hpZWQsX19fc3lzY2FsbF9mY250bDY0LF9fX3N5c2NhbGxfZnN0YXQ2NCxfX19zeXNjYWxsX2lvY3RsLF9fX3N5c2NhbGxfbHN0YXQ2NCxfX19zeXNjYWxsX25ld2ZzdGF0YXQsX19fc3lzY2FsbF9vcGVuYXQsX19fc3lzY2FsbF9zdGF0NjQsX19tbWFwX2pzLF9fbXVubWFwX2pzLF9lbnZpcm9uX2dldCxfZW52aXJvbl9zaXplc19nZXQsX2ZkX2Nsb3NlLF9mZF9yZWFkLF9mZF9zZWVrLF9mZF93cml0ZV07ZnVuY3Rpb24gY2hlY2tJbmNvbWluZ01vZHVsZUFQSSgpe2lnbm9yZWRNb2R1bGVQcm9wKCJmZXRjaFNldHRpbmdzIil9dmFyIEFTTV9DT05TVFM9ezU0MDkzMjooKT0+dHlwZW9mIHdhc21PZmZzZXRDb252ZXJ0ZXIhPT0idW5kZWZpbmVkIn07ZnVuY3Rpb24gSGF2ZU9mZnNldENvbnZlcnRlcigpe3JldHVybiB0eXBlb2Ygd2FzbU9mZnNldENvbnZlcnRlciE9PSJ1bmRlZmluZWQifXZhciBfX19nZXRUeXBlTmFtZT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX19nZXRUeXBlTmFtZSIpO3ZhciBfX2VtYmluZF9pbml0aWFsaXplX2JpbmRpbmdzPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1iaW5kX2luaXRpYWxpemVfYmluZGluZ3MiKTt2YXIgX2dldF9jcF9tb2RlbF9zY2hlbWE9TW9kdWxlWyJfZ2V0X2NwX21vZGVsX3NjaGVtYSJdPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9nZXRfY3BfbW9kZWxfc2NoZW1hIik7dmFyIF9nZXRfc2F0X3BhcmFtZXRlcnNfc2NoZW1hPU1vZHVsZVsiX2dldF9zYXRfcGFyYW1ldGVyc19zY2hlbWEiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZ2V0X3NhdF9wYXJhbWV0ZXJzX3NjaGVtYSIpO3ZhciBfc29sdmVfbW9kZWw9TW9kdWxlWyJfc29sdmVfbW9kZWwiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfc29sdmVfbW9kZWwiKTt2YXIgX21hbGxvYz1Nb2R1bGVbIl9tYWxsb2MiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfbWFsbG9jIik7dmFyIF9mcmVlX2J1ZmZlcj1Nb2R1bGVbIl9mcmVlX2J1ZmZlciJdPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9mcmVlX2J1ZmZlciIpO3ZhciBfZnJlZT1Nb2R1bGVbIl9mcmVlIl09bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2ZyZWUiKTt2YXIgX2ludGVycnVwdF9zb2x2ZT1Nb2R1bGVbIl9pbnRlcnJ1cHRfc29sdmUiXT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfaW50ZXJydXB0X3NvbHZlIik7dmFyIF92YWxpZGF0ZV9tb2RlbD1Nb2R1bGVbIl92YWxpZGF0ZV9tb2RlbCJdPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl92YWxpZGF0ZV9tb2RlbCIpO3ZhciBfcHRocmVhZF9zZWxmPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9wdGhyZWFkX3NlbGYiKTt2YXIgX2ZmbHVzaD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZmZsdXNoIik7dmFyIF9zdHJlcnJvcj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfc3RyZXJyb3IiKTt2YXIgX19lbXNjcmlwdGVuX3Rsc19pbml0PW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl90bHNfaW5pdCIpO3ZhciBfZW1zY3JpcHRlbl9idWlsdGluX21lbWFsaWduPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9lbXNjcmlwdGVuX2J1aWx0aW5fbWVtYWxpZ24iKTt2YXIgX19lbXNjcmlwdGVuX3RocmVhZF9pbml0PW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl90aHJlYWRfaW5pdCIpO3ZhciBfX2Vtc2NyaXB0ZW5fdGhyZWFkX2NyYXNoZWQ9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX19lbXNjcmlwdGVuX3RocmVhZF9jcmFzaGVkIik7dmFyIF9lbXNjcmlwdGVuX3N0YWNrX2dldF9lbmQ9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZCIpO3ZhciBfZW1zY3JpcHRlbl9zdGFja19nZXRfYmFzZT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZW1zY3JpcHRlbl9zdGFja19nZXRfYmFzZSIpO3ZhciBfX2Vtc2NyaXB0ZW5fcnVuX2pzX29uX21haW5fdGhyZWFkPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl9ydW5fanNfb25fbWFpbl90aHJlYWQiKTt2YXIgX19lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGE9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX19lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGEiKTt2YXIgX19lbXNjcmlwdGVuX3RocmVhZF9leGl0PW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl90aHJlYWRfZXhpdCIpO3ZhciBfX2Vtc2NyaXB0ZW5fY2hlY2tfbWFpbGJveD1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fY2hlY2tfbWFpbGJveCIpO3ZhciBfZW1zY3JpcHRlbl9zdGFja19pbml0PW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9lbXNjcmlwdGVuX3N0YWNrX2luaXQiKTt2YXIgX2Vtc2NyaXB0ZW5fc3RhY2tfc2V0X2xpbWl0cz1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfZW1zY3JpcHRlbl9zdGFja19zZXRfbGltaXRzIik7dmFyIF9lbXNjcmlwdGVuX3N0YWNrX2dldF9mcmVlPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9lbXNjcmlwdGVuX3N0YWNrX2dldF9mcmVlIik7dmFyIF9fZW1zY3JpcHRlbl9zdGFja19yZXN0b3JlPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9fZW1zY3JpcHRlbl9zdGFja19yZXN0b3JlIik7dmFyIF9fZW1zY3JpcHRlbl9zdGFja19hbGxvYz1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJfX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2MiKTt2YXIgX2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2N1cnJlbnQ9bWFrZUludmFsaWRFYXJseUFjY2VzcygiX2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2N1cnJlbnQiKTt2YXIgZHluQ2FsbF9paT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2lpIik7dmFyIGR5bkNhbGxfdj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3YiKTt2YXIgZHluQ2FsbF92aT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpIik7dmFyIGR5bkNhbGxfaWlpPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfaWlpIik7dmFyIGR5bkNhbGxfaWlpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2lpaWkiKTt2YXIgZHluQ2FsbF92aWk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF92aWkiKTt2YXIgZHluQ2FsbF9paWlpamlqPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfaWlpaWppaiIpO3ZhciBkeW5DYWxsX3ZpaWk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF92aWlpIik7dmFyIGR5bkNhbGxfaWlpaWk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9paWlpaSIpO3ZhciBkeW5DYWxsX3ZpaWlpPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfdmlpaWkiKTt2YXIgZHluQ2FsbF92aWlpaWlpPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfdmlpaWlpaSIpO3ZhciBkeW5DYWxsX3ZpaWlpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpaWlpaSIpO3ZhciBkeW5DYWxsX3ZpaWlpaj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpaWlpaiIpO3ZhciBkeW5DYWxsX2ppPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfamkiKTt2YXIgZHluQ2FsbF92aWlqPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfdmlpaiIpO3ZhciBkeW5DYWxsX2k9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9pIik7dmFyIGR5bkNhbGxfZGk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9kaSIpO3ZhciBkeW5DYWxsX3ZpaWppaWlpPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfdmlpamlpaWkiKTt2YXIgZHluQ2FsbF9qaWk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9qaWkiKTt2YXIgZHluQ2FsbF92aWppPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfdmlqaSIpO3ZhciBkeW5DYWxsX3ZpZmk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF92aWZpIik7dmFyIGR5bkNhbGxfdmlkaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpZGkiKTt2YXIgZHluQ2FsbF92aWlpaWlpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpaWlpaWlpIik7dmFyIGR5bkNhbGxfaWlpaWlpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2lpaWlpaWkiKTt2YXIgZHluQ2FsbF92aWlqaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpaWppIik7dmFyIGR5bkNhbGxfamlpamk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9qaWlqaSIpO3ZhciBkeW5DYWxsX2Rpaj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2RpaiIpO3ZhciBkeW5DYWxsX2RpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2RpaSIpO3ZhciBkeW5DYWxsX2ZpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2ZpaSIpO3ZhciBkeW5DYWxsX3ZpaWppaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX3ZpaWppaSIpO3ZhciBkeW5DYWxsX2lpaWlpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2lpaWlpaSIpO3ZhciBkeW5DYWxsX3ZqPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfdmoiKTt2YXIgZHluQ2FsbF92aWo9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF92aWoiKTt2YXIgZHluQ2FsbF9qaWppPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfamlqaSIpO3ZhciBkeW5DYWxsX2lpZGlpaWk9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9paWRpaWlpIik7dmFyIGR5bkNhbGxfaWlpaWlpaWlpPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfaWlpaWlpaWlpIik7dmFyIGR5bkNhbGxfaWlpaWlqPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfaWlpaWlqIik7dmFyIGR5bkNhbGxfaWlpaWlkPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoImR5bkNhbGxfaWlpaWlkIik7dmFyIGR5bkNhbGxfaWlpaWlqaj1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2lpaWlpamoiKTt2YXIgZHluQ2FsbF9paWlpaWlpaT1tYWtlSW52YWxpZEVhcmx5QWNjZXNzKCJkeW5DYWxsX2lpaWlpaWlpIik7dmFyIGR5bkNhbGxfaWlpaWlpamo9bWFrZUludmFsaWRFYXJseUFjY2VzcygiZHluQ2FsbF9paWlpaWlqaiIpO3ZhciBfYXN5bmNpZnlfc3RhcnRfdW53aW5kPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9hc3luY2lmeV9zdGFydF91bndpbmQiKTt2YXIgX2FzeW5jaWZ5X3N0b3BfdW53aW5kPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9hc3luY2lmeV9zdG9wX3Vud2luZCIpO3ZhciBfYXN5bmNpZnlfc3RhcnRfcmV3aW5kPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9hc3luY2lmeV9zdGFydF9yZXdpbmQiKTt2YXIgX2FzeW5jaWZ5X3N0b3BfcmV3aW5kPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9hc3luY2lmeV9zdG9wX3Jld2luZCIpO3ZhciBfX2luZGlyZWN0X2Z1bmN0aW9uX3RhYmxlPW1ha2VJbnZhbGlkRWFybHlBY2Nlc3MoIl9faW5kaXJlY3RfZnVuY3Rpb25fdGFibGUiKTtmdW5jdGlvbiBhc3NpZ25XYXNtRXhwb3J0cyh3YXNtRXhwb3J0cyl7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siX19nZXRUeXBlTmFtZSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX19nZXRUeXBlTmFtZSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbWJpbmRfaW5pdGlhbGl6ZV9iaW5kaW5ncyJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2VtYmluZF9pbml0aWFsaXplX2JpbmRpbmdzIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZ2V0X2NwX21vZGVsX3NjaGVtYSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZ2V0X2NwX21vZGVsX3NjaGVtYSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImdldF9zYXRfcGFyYW1ldGVyc19zY2hlbWEiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGdldF9zYXRfcGFyYW1ldGVyc19zY2hlbWEiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJzb2x2ZV9tb2RlbCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogc29sdmVfbW9kZWwiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJtYWxsb2MiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IG1hbGxvYyIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImZyZWVfYnVmZmVyIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBmcmVlX2J1ZmZlciIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImZyZWUiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGZyZWUiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJpbnRlcnJ1cHRfc29sdmUiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGludGVycnVwdF9zb2x2ZSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbInZhbGlkYXRlX21vZGVsIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiB2YWxpZGF0ZV9tb2RlbCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbInB0aHJlYWRfc2VsZiJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogcHRocmVhZF9zZWxmIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZmZsdXNoIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBmZmx1c2giKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJzdHJlcnJvciJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogc3RyZXJyb3IiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl90bHNfaW5pdCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fdGxzX2luaXQiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX2J1aWx0aW5fbWVtYWxpZ24iXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGVtc2NyaXB0ZW5fYnVpbHRpbl9tZW1hbGlnbiIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX3RocmVhZF9pbml0Il0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBfZW1zY3JpcHRlbl90aHJlYWRfaW5pdCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX3RocmVhZF9jcmFzaGVkIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBfZW1zY3JpcHRlbl90aHJlYWRfY3Jhc2hlZCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZW1zY3JpcHRlbl9zdGFja19nZXRfZW5kIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19nZXRfYmFzZSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZW1zY3JpcHRlbl9zdGFja19nZXRfYmFzZSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fcnVuX2pzX29uX21haW5fdGhyZWFkIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siX2Vtc2NyaXB0ZW5fdGhyZWFkX2ZyZWVfZGF0YSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fdGhyZWFkX2ZyZWVfZGF0YSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX3RocmVhZF9leGl0Il0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBfZW1zY3JpcHRlbl90aHJlYWRfZXhpdCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX2NoZWNrX21haWxib3giXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbXNjcmlwdGVuX2NoZWNrX21haWxib3giKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX2luaXQiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGVtc2NyaXB0ZW5fc3RhY2tfaW5pdCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfc2V0X2xpbWl0cyJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZW1zY3JpcHRlbl9zdGFja19zZXRfbGltaXRzIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19nZXRfZnJlZSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZW1zY3JpcHRlbl9zdGFja19nZXRfZnJlZSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbIl9lbXNjcmlwdGVuX3N0YWNrX3Jlc3RvcmUiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IF9lbXNjcmlwdGVuX3N0YWNrX3Jlc3RvcmUiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl9zdGFja19hbGxvYyJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX2Vtc2NyaXB0ZW5fc3RhY2tfYWxsb2MiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50Il0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBlbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50Iik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF9paSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9paSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfdiJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92Iik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2lpaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9paWlpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfdmlpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF9paWlpamlqIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2lpaWlqaWoiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpaWkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfdmlpaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfaWlpaWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpaWlpIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX3ZpaWlpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWlpaWlpIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX3ZpaWlpaWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpaWlpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aWlpaWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpaWlpaiJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aWlpaWoiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2ppIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2ppIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWlqIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX3ZpaWoiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2kiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfZGkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfZGkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpaWppaWlpIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX3ZpaWppaWlpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF9qaWkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfamlpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWppIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX3ZpamkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpZmkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfdmlmaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfdmlkaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aWRpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWlpaWlpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aWlpaWlpaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWlpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9paWlpaWlpIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWlqaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aWlqaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfamlpamkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfamlpamkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2RpaiJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9kaWoiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2RpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9kaWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2ZpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9maWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZpaWppaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF92aWlqaWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2lpaWlpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9paWlpaWkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX3ZqIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX3ZqIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF92aWoiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfdmlqIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF9qaWppIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2ppamkiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJkeW5DYWxsX2lpZGlpaWkiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfaWlkaWlpaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWlpaWlpIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2lpaWlpaWlpaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWlqIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2lpaWlpaiIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWlkIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBkeW5DYWxsX2lpaWlpZCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWlqaiJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9paWlpaWpqIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siZHluQ2FsbF9paWlpaWlpaSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogZHluQ2FsbF9paWlpaWlpaSIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImR5bkNhbGxfaWlpaWlpamoiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGR5bkNhbGxfaWlpaWlpamoiKTthc3NlcnQodHlwZW9mIHdhc21FeHBvcnRzWyJhc3luY2lmeV9zdGFydF91bndpbmQiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGFzeW5jaWZ5X3N0YXJ0X3Vud2luZCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImFzeW5jaWZ5X3N0b3BfdW53aW5kIl0hPSJ1bmRlZmluZWQiLCJtaXNzaW5nIFdhc20gZXhwb3J0OiBhc3luY2lmeV9zdG9wX3Vud2luZCIpO2Fzc2VydCh0eXBlb2Ygd2FzbUV4cG9ydHNbImFzeW5jaWZ5X3N0YXJ0X3Jld2luZCJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogYXN5bmNpZnlfc3RhcnRfcmV3aW5kIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siYXN5bmNpZnlfc3RvcF9yZXdpbmQiXSE9InVuZGVmaW5lZCIsIm1pc3NpbmcgV2FzbSBleHBvcnQ6IGFzeW5jaWZ5X3N0b3BfcmV3aW5kIik7YXNzZXJ0KHR5cGVvZiB3YXNtRXhwb3J0c1siX19pbmRpcmVjdF9mdW5jdGlvbl90YWJsZSJdIT0idW5kZWZpbmVkIiwibWlzc2luZyBXYXNtIGV4cG9ydDogX19pbmRpcmVjdF9mdW5jdGlvbl90YWJsZSIpO19fX2dldFR5cGVOYW1lPWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9fZ2V0VHlwZU5hbWUiLDEpO19fZW1iaW5kX2luaXRpYWxpemVfYmluZGluZ3M9Y3JlYXRlRXhwb3J0V3JhcHBlcigiX2VtYmluZF9pbml0aWFsaXplX2JpbmRpbmdzIiwwKTtfZ2V0X2NwX21vZGVsX3NjaGVtYT1Nb2R1bGVbIl9nZXRfY3BfbW9kZWxfc2NoZW1hIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZ2V0X2NwX21vZGVsX3NjaGVtYSIsMCk7X2dldF9zYXRfcGFyYW1ldGVyc19zY2hlbWE9TW9kdWxlWyJfZ2V0X3NhdF9wYXJhbWV0ZXJzX3NjaGVtYSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImdldF9zYXRfcGFyYW1ldGVyc19zY2hlbWEiLDApO19zb2x2ZV9tb2RlbD1Nb2R1bGVbIl9zb2x2ZV9tb2RlbCJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoInNvbHZlX21vZGVsIiw1KTtfbWFsbG9jPU1vZHVsZVsiX21hbGxvYyJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoIm1hbGxvYyIsMSk7X2ZyZWVfYnVmZmVyPU1vZHVsZVsiX2ZyZWVfYnVmZmVyIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZnJlZV9idWZmZXIiLDEpO19mcmVlPU1vZHVsZVsiX2ZyZWUiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJmcmVlIiwxKTtfaW50ZXJydXB0X3NvbHZlPU1vZHVsZVsiX2ludGVycnVwdF9zb2x2ZSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImludGVycnVwdF9zb2x2ZSIsMCk7X3ZhbGlkYXRlX21vZGVsPU1vZHVsZVsiX3ZhbGlkYXRlX21vZGVsIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigidmFsaWRhdGVfbW9kZWwiLDMpO19wdGhyZWFkX3NlbGY9Y3JlYXRlRXhwb3J0V3JhcHBlcigicHRocmVhZF9zZWxmIiwwKTtfZmZsdXNoPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImZmbHVzaCIsMSk7X3N0cmVycm9yPWNyZWF0ZUV4cG9ydFdyYXBwZXIoInN0cmVycm9yIiwxKTtfX2Vtc2NyaXB0ZW5fdGxzX2luaXQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiX2Vtc2NyaXB0ZW5fdGxzX2luaXQiLDApO19lbXNjcmlwdGVuX2J1aWx0aW5fbWVtYWxpZ249Y3JlYXRlRXhwb3J0V3JhcHBlcigiZW1zY3JpcHRlbl9idWlsdGluX21lbWFsaWduIiwyKTtfX2Vtc2NyaXB0ZW5fdGhyZWFkX2luaXQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiX2Vtc2NyaXB0ZW5fdGhyZWFkX2luaXQiLDYpO19fZW1zY3JpcHRlbl90aHJlYWRfY3Jhc2hlZD1jcmVhdGVFeHBvcnRXcmFwcGVyKCJfZW1zY3JpcHRlbl90aHJlYWRfY3Jhc2hlZCIsMCk7X2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2VuZD13YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19nZXRfZW5kIl07X2Vtc2NyaXB0ZW5fc3RhY2tfZ2V0X2Jhc2U9d2FzbUV4cG9ydHNbImVtc2NyaXB0ZW5fc3RhY2tfZ2V0X2Jhc2UiXTtfX2Vtc2NyaXB0ZW5fcnVuX2pzX29uX21haW5fdGhyZWFkPWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9lbXNjcmlwdGVuX3J1bl9qc19vbl9tYWluX3RocmVhZCIsNSk7X19lbXNjcmlwdGVuX3RocmVhZF9mcmVlX2RhdGE9Y3JlYXRlRXhwb3J0V3JhcHBlcigiX2Vtc2NyaXB0ZW5fdGhyZWFkX2ZyZWVfZGF0YSIsMSk7X19lbXNjcmlwdGVuX3RocmVhZF9leGl0PWNyZWF0ZUV4cG9ydFdyYXBwZXIoIl9lbXNjcmlwdGVuX3RocmVhZF9leGl0IiwxKTtfX2Vtc2NyaXB0ZW5fY2hlY2tfbWFpbGJveD1jcmVhdGVFeHBvcnRXcmFwcGVyKCJfZW1zY3JpcHRlbl9jaGVja19tYWlsYm94IiwwKTtfZW1zY3JpcHRlbl9zdGFja19pbml0PXdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX2luaXQiXTtfZW1zY3JpcHRlbl9zdGFja19zZXRfbGltaXRzPXdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX3NldF9saW1pdHMiXTtfZW1zY3JpcHRlbl9zdGFja19nZXRfZnJlZT13YXNtRXhwb3J0c1siZW1zY3JpcHRlbl9zdGFja19nZXRfZnJlZSJdO19fZW1zY3JpcHRlbl9zdGFja19yZXN0b3JlPXdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl9zdGFja19yZXN0b3JlIl07X19lbXNjcmlwdGVuX3N0YWNrX2FsbG9jPXdhc21FeHBvcnRzWyJfZW1zY3JpcHRlbl9zdGFja19hbGxvYyJdO19lbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50PXdhc21FeHBvcnRzWyJlbXNjcmlwdGVuX3N0YWNrX2dldF9jdXJyZW50Il07ZHluQ2FsbF9paT1keW5DYWxsc1siaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX2lpIiwyKTtkeW5DYWxsX3Y9ZHluQ2FsbHNbInYiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3YiLDEpO2R5bkNhbGxfdmk9ZHluQ2FsbHNbInZpIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF92aSIsMik7ZHluQ2FsbF9paWk9ZHluQ2FsbHNbImlpaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfaWlpIiwzKTtkeW5DYWxsX2lpaWk9ZHluQ2FsbHNbImlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX2lpaWkiLDQpO2R5bkNhbGxfdmlpPWR5bkNhbGxzWyJ2aWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaSIsMyk7ZHluQ2FsbF9paWlpamlqPWR5bkNhbGxzWyJpaWlpamlqIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF9paWlpamlqIiw3KTtkeW5DYWxsX3ZpaWk9ZHluQ2FsbHNbInZpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaWkiLDQpO2R5bkNhbGxfaWlpaWk9ZHluQ2FsbHNbImlpaWlpIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF9paWlpaSIsNSk7ZHluQ2FsbF92aWlpaT1keW5DYWxsc1sidmlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaWlpIiw1KTtkeW5DYWxsX3ZpaWlpaWk9ZHluQ2FsbHNbInZpaWlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaWlpaWkiLDcpO2R5bkNhbGxfdmlpaWlpPWR5bkNhbGxzWyJ2aWlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaWlpaSIsNik7ZHluQ2FsbF92aWlpaWo9ZHluQ2FsbHNbInZpaWlpaiJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfdmlpaWlqIiw2KTtkeW5DYWxsX2ppPWR5bkNhbGxzWyJqaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfamkiLDIpO2R5bkNhbGxfdmlpaj1keW5DYWxsc1sidmlpaiJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfdmlpaiIsNCk7ZHluQ2FsbF9pPWR5bkNhbGxzWyJpIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF9pIiwxKTtkeW5DYWxsX2RpPWR5bkNhbGxzWyJkaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfZGkiLDIpO2R5bkNhbGxfdmlpamlpaWk9ZHluQ2FsbHNbInZpaWppaWlpIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF92aWlqaWlpaSIsOCk7ZHluQ2FsbF9qaWk9ZHluQ2FsbHNbImppaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfamlpIiwzKTtkeW5DYWxsX3Zpamk9ZHluQ2FsbHNbInZpamkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpamkiLDQpO2R5bkNhbGxfdmlmaT1keW5DYWxsc1sidmlmaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfdmlmaSIsNCk7ZHluQ2FsbF92aWRpPWR5bkNhbGxzWyJ2aWRpIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF92aWRpIiw0KTtkeW5DYWxsX3ZpaWlpaWlpPWR5bkNhbGxzWyJ2aWlpaWlpaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfdmlpaWlpaWkiLDgpO2R5bkNhbGxfaWlpaWlpaT1keW5DYWxsc1siaWlpaWlpaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfaWlpaWlpaSIsNyk7ZHluQ2FsbF92aWlqaT1keW5DYWxsc1sidmlpamkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaWppIiw1KTtkeW5DYWxsX2ppaWppPWR5bkNhbGxzWyJqaWlqaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfamlpamkiLDUpO2R5bkNhbGxfZGlqPWR5bkNhbGxzWyJkaWoiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX2RpaiIsMyk7ZHluQ2FsbF9kaWk9ZHluQ2FsbHNbImRpaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfZGlpIiwzKTtkeW5DYWxsX2ZpaT1keW5DYWxsc1siZmlpIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF9maWkiLDMpO2R5bkNhbGxfdmlpamlpPWR5bkNhbGxzWyJ2aWlqaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaWppaSIsNik7ZHluQ2FsbF9paWlpaWk9ZHluQ2FsbHNbImlpaWlpaSJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfaWlpaWlpIiw2KTtkeW5DYWxsX3ZqPWR5bkNhbGxzWyJ2aiJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfdmoiLDIpO2R5bkNhbGxfdmlqPWR5bkNhbGxzWyJ2aWoiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX3ZpaiIsMyk7ZHluQ2FsbF9qaWppPWR5bkNhbGxzWyJqaWppIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF9qaWppIiw0KTtkeW5DYWxsX2lpZGlpaWk9ZHluQ2FsbHNbImlpZGlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX2lpZGlpaWkiLDcpO2R5bkNhbGxfaWlpaWlpaWlpPWR5bkNhbGxzWyJpaWlpaWlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX2lpaWlpaWlpaSIsOSk7ZHluQ2FsbF9paWlpaWo9ZHluQ2FsbHNbImlpaWlpaiJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfaWlpaWlqIiw2KTtkeW5DYWxsX2lpaWlpZD1keW5DYWxsc1siaWlpaWlkIl09Y3JlYXRlRXhwb3J0V3JhcHBlcigiZHluQ2FsbF9paWlpaWQiLDYpO2R5bkNhbGxfaWlpaWlqaj1keW5DYWxsc1siaWlpaWlqaiJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfaWlpaWlqaiIsNyk7ZHluQ2FsbF9paWlpaWlpaT1keW5DYWxsc1siaWlpaWlpaWkiXT1jcmVhdGVFeHBvcnRXcmFwcGVyKCJkeW5DYWxsX2lpaWlpaWlpIiw4KTtkeW5DYWxsX2lpaWlpaWpqPWR5bkNhbGxzWyJpaWlpaWlqaiJdPWNyZWF0ZUV4cG9ydFdyYXBwZXIoImR5bkNhbGxfaWlpaWlpamoiLDgpO19hc3luY2lmeV9zdGFydF91bndpbmQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiYXN5bmNpZnlfc3RhcnRfdW53aW5kIiwxKTtfYXN5bmNpZnlfc3RvcF91bndpbmQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiYXN5bmNpZnlfc3RvcF91bndpbmQiLDApO19hc3luY2lmeV9zdGFydF9yZXdpbmQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiYXN5bmNpZnlfc3RhcnRfcmV3aW5kIiwxKTtfYXN5bmNpZnlfc3RvcF9yZXdpbmQ9Y3JlYXRlRXhwb3J0V3JhcHBlcigiYXN5bmNpZnlfc3RvcF9yZXdpbmQiLDApO19faW5kaXJlY3RfZnVuY3Rpb25fdGFibGU9d2FzbUV4cG9ydHNbIl9faW5kaXJlY3RfZnVuY3Rpb25fdGFibGUiXX12YXIgd2FzbUltcG9ydHM7ZnVuY3Rpb24gYXNzaWduV2FzbUltcG9ydHMoKXt3YXNtSW1wb3J0cz17SGF2ZU9mZnNldENvbnZlcnRlcixfX2Fzc2VydF9mYWlsOl9fX2Fzc2VydF9mYWlsLF9fY3hhX3Rocm93Ol9fX2N4YV90aHJvdyxfX3B0aHJlYWRfY3JlYXRlX2pzOl9fX3B0aHJlYWRfY3JlYXRlX2pzLF9fc3lzY2FsbF9mY250bDY0Ol9fX3N5c2NhbGxfZmNudGw2NCxfX3N5c2NhbGxfZnN0YXQ2NDpfX19zeXNjYWxsX2ZzdGF0NjQsX19zeXNjYWxsX2lvY3RsOl9fX3N5c2NhbGxfaW9jdGwsX19zeXNjYWxsX2xzdGF0NjQ6X19fc3lzY2FsbF9sc3RhdDY0LF9fc3lzY2FsbF9uZXdmc3RhdGF0Ol9fX3N5c2NhbGxfbmV3ZnN0YXRhdCxfX3N5c2NhbGxfb3BlbmF0Ol9fX3N5c2NhbGxfb3BlbmF0LF9fc3lzY2FsbF9zdGF0NjQ6X19fc3lzY2FsbF9zdGF0NjQsX2Fib3J0X2pzOl9fYWJvcnRfanMsX2VtYmluZF9yZWdpc3Rlcl9iaWdpbnQ6X19lbWJpbmRfcmVnaXN0ZXJfYmlnaW50LF9lbWJpbmRfcmVnaXN0ZXJfYm9vbDpfX2VtYmluZF9yZWdpc3Rlcl9ib29sLF9lbWJpbmRfcmVnaXN0ZXJfZW12YWw6X19lbWJpbmRfcmVnaXN0ZXJfZW12YWwsX2VtYmluZF9yZWdpc3Rlcl9mbG9hdDpfX2VtYmluZF9yZWdpc3Rlcl9mbG9hdCxfZW1iaW5kX3JlZ2lzdGVyX2Z1bmN0aW9uOl9fZW1iaW5kX3JlZ2lzdGVyX2Z1bmN0aW9uLF9lbWJpbmRfcmVnaXN0ZXJfaW50ZWdlcjpfX2VtYmluZF9yZWdpc3Rlcl9pbnRlZ2VyLF9lbWJpbmRfcmVnaXN0ZXJfbWVtb3J5X3ZpZXc6X19lbWJpbmRfcmVnaXN0ZXJfbWVtb3J5X3ZpZXcsX2VtYmluZF9yZWdpc3Rlcl9zdGRfc3RyaW5nOl9fZW1iaW5kX3JlZ2lzdGVyX3N0ZF9zdHJpbmcsX2VtYmluZF9yZWdpc3Rlcl9zdGRfd3N0cmluZzpfX2VtYmluZF9yZWdpc3Rlcl9zdGRfd3N0cmluZyxfZW1iaW5kX3JlZ2lzdGVyX3ZvaWQ6X19lbWJpbmRfcmVnaXN0ZXJfdm9pZCxfZW1zY3JpcHRlbl9pbml0X21haW5fdGhyZWFkX2pzOl9fZW1zY3JpcHRlbl9pbml0X21haW5fdGhyZWFkX2pzLF9lbXNjcmlwdGVuX25vdGlmeV9tYWlsYm94X3Bvc3RtZXNzYWdlOl9fZW1zY3JpcHRlbl9ub3RpZnlfbWFpbGJveF9wb3N0bWVzc2FnZSxfZW1zY3JpcHRlbl9yZWNlaXZlX29uX21haW5fdGhyZWFkX2pzOl9fZW1zY3JpcHRlbl9yZWNlaXZlX29uX21haW5fdGhyZWFkX2pzLF9lbXNjcmlwdGVuX3RocmVhZF9jbGVhbnVwOl9fZW1zY3JpcHRlbl90aHJlYWRfY2xlYW51cCxfZW1zY3JpcHRlbl90aHJlYWRfbWFpbGJveF9hd2FpdDpfX2Vtc2NyaXB0ZW5fdGhyZWFkX21haWxib3hfYXdhaXQsX2Vtc2NyaXB0ZW5fdGhyZWFkX3NldF9zdHJvbmdyZWY6X19lbXNjcmlwdGVuX3RocmVhZF9zZXRfc3Ryb25ncmVmLF9tbWFwX2pzOl9fbW1hcF9qcyxfbXVubWFwX2pzOl9fbXVubWFwX2pzLF90enNldF9qczpfX3R6c2V0X2pzLGNsb2NrX3RpbWVfZ2V0Ol9jbG9ja190aW1lX2dldCxlbXNjcmlwdGVuX2FzbV9jb25zdF9pbnQ6X2Vtc2NyaXB0ZW5fYXNtX2NvbnN0X2ludCxlbXNjcmlwdGVuX2NoZWNrX2Jsb2NraW5nX2FsbG93ZWQ6X2Vtc2NyaXB0ZW5fY2hlY2tfYmxvY2tpbmdfYWxsb3dlZCxlbXNjcmlwdGVuX2Vycm46X2Vtc2NyaXB0ZW5fZXJybixlbXNjcmlwdGVuX2V4aXRfd2l0aF9saXZlX3J1bnRpbWU6X2Vtc2NyaXB0ZW5fZXhpdF93aXRoX2xpdmVfcnVudGltZSxlbXNjcmlwdGVuX2dldF9oZWFwX21heDpfZW1zY3JpcHRlbl9nZXRfaGVhcF9tYXgsZW1zY3JpcHRlbl9nZXRfbm93Ol9lbXNjcmlwdGVuX2dldF9ub3csZW1zY3JpcHRlbl9udW1fbG9naWNhbF9jb3JlczpfZW1zY3JpcHRlbl9udW1fbG9naWNhbF9jb3JlcyxlbXNjcmlwdGVuX3BjX2dldF9mdW5jdGlvbjpfZW1zY3JpcHRlbl9wY19nZXRfZnVuY3Rpb24sZW1zY3JpcHRlbl9yZXNpemVfaGVhcDpfZW1zY3JpcHRlbl9yZXNpemVfaGVhcCxlbXNjcmlwdGVuX3N0YWNrX3NuYXBzaG90Ol9lbXNjcmlwdGVuX3N0YWNrX3NuYXBzaG90LGVtc2NyaXB0ZW5fc3RhY2tfdW53aW5kX2J1ZmZlcjpfZW1zY3JpcHRlbl9zdGFja191bndpbmRfYnVmZmVyLGVudmlyb25fZ2V0Ol9lbnZpcm9uX2dldCxlbnZpcm9uX3NpemVzX2dldDpfZW52aXJvbl9zaXplc19nZXQsZXhpdDpfZXhpdCxmZF9jbG9zZTpfZmRfY2xvc2UsZmRfcmVhZDpfZmRfcmVhZCxmZF9zZWVrOl9mZF9zZWVrLGZkX3dyaXRlOl9mZF93cml0ZSxtZW1vcnk6d2FzbU1lbW9yeSxwcm9jX2V4aXQ6X3Byb2NfZXhpdCxyYW5kb21fZ2V0Ol9yYW5kb21fZ2V0fX12YXIgY2FsbGVkUnVuO2Z1bmN0aW9uIHN0YWNrQ2hlY2tJbml0KCl7YXNzZXJ0KCFFTlZJUk9OTUVOVF9JU19QVEhSRUFEKTtfZW1zY3JpcHRlbl9zdGFja19pbml0KCk7d3JpdGVTdGFja0Nvb2tpZSgpfWZ1bmN0aW9uIHJ1bigpe2lmKHJ1bkRlcGVuZGVuY2llcz4wKXtkZXBlbmRlbmNpZXNGdWxmaWxsZWQ9cnVuO3JldHVybn1pZihFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXtyZWFkeVByb21pc2VSZXNvbHZlPy4oTW9kdWxlKTtpbml0UnVudGltZSgpO3JldHVybn1zdGFja0NoZWNrSW5pdCgpO3ByZVJ1bigpO2lmKHJ1bkRlcGVuZGVuY2llcz4wKXtkZXBlbmRlbmNpZXNGdWxmaWxsZWQ9cnVuO3JldHVybn1mdW5jdGlvbiBkb1J1bigpe2Fzc2VydCghY2FsbGVkUnVuKTtjYWxsZWRSdW49dHJ1ZTtNb2R1bGVbImNhbGxlZFJ1biJdPXRydWU7aWYoQUJPUlQpcmV0dXJuO2luaXRSdW50aW1lKCk7cmVhZHlQcm9taXNlUmVzb2x2ZT8uKE1vZHVsZSk7TW9kdWxlWyJvblJ1bnRpbWVJbml0aWFsaXplZCJdPy4oKTtjb25zdW1lZE1vZHVsZVByb3AoIm9uUnVudGltZUluaXRpYWxpemVkIik7YXNzZXJ0KCFNb2R1bGVbIl9tYWluIl0sJ2NvbXBpbGVkIHdpdGhvdXQgYSBtYWluLCBidXQgb25lIGlzIHByZXNlbnQuIGlmIHlvdSBhZGRlZCBpdCBmcm9tIEpTLCB1c2UgTW9kdWxlWyJvblJ1bnRpbWVJbml0aWFsaXplZCJdJyk7cG9zdFJ1bigpfWlmKE1vZHVsZVsic2V0U3RhdHVzIl0pe01vZHVsZVsic2V0U3RhdHVzIl0oIlJ1bm5pbmcuLi4iKTtzZXRUaW1lb3V0KCgpPT57c2V0VGltZW91dCgoKT0+TW9kdWxlWyJzZXRTdGF0dXMiXSgiIiksMSk7ZG9SdW4oKX0sMSl9ZWxzZXtkb1J1bigpfWNoZWNrU3RhY2tDb29raWUoKX1mdW5jdGlvbiBjaGVja1VuZmx1c2hlZENvbnRlbnQoKXt2YXIgb2xkT3V0PW91dDt2YXIgb2xkRXJyPWVycjt2YXIgaGFzPWZhbHNlO291dD1lcnI9eD0+e2hhcz10cnVlfTt0cnl7X2ZmbHVzaCgwKTtmb3IodmFyIG5hbWUgb2ZbInN0ZG91dCIsInN0ZGVyciJdKXt2YXIgaW5mbz1GUy5hbmFseXplUGF0aCgiL2Rldi8iK25hbWUpO2lmKCFpbmZvKXJldHVybjt2YXIgc3RyZWFtPWluZm8ub2JqZWN0O3ZhciByZGV2PXN0cmVhbS5yZGV2O3ZhciB0dHk9VFRZLnR0eXNbcmRldl07aWYodHR5Py5vdXRwdXQ/Lmxlbmd0aCl7aGFzPXRydWV9fX1jYXRjaChlKXt9b3V0PW9sZE91dDtlcnI9b2xkRXJyO2lmKGhhcyl7d2Fybk9uY2UoInN0ZGlvIHN0cmVhbXMgaGFkIGNvbnRlbnQgaW4gdGhlbSB0aGF0IHdhcyBub3QgZmx1c2hlZC4geW91IHNob3VsZCBzZXQgRVhJVF9SVU5USU1FIHRvIDEgKHNlZSB0aGUgRW1zY3JpcHRlbiBGQVEpLCBvciBtYWtlIHN1cmUgdG8gZW1pdCBhIG5ld2xpbmUgd2hlbiB5b3UgcHJpbnRmIGV0Yy4iKX19dmFyIHdhc21FeHBvcnRzO2lmKCFFTlZJUk9OTUVOVF9JU19QVEhSRUFEKXt3YXNtRXhwb3J0cz1hd2FpdCAoY3JlYXRlV2FzbSgpKTtydW4oKX1pZihydW50aW1lSW5pdGlhbGl6ZWQpe21vZHVsZVJ0bj1Nb2R1bGV9ZWxzZXttb2R1bGVSdG49bmV3IFByb21pc2UoKHJlc29sdmUscmVqZWN0KT0+e3JlYWR5UHJvbWlzZVJlc29sdmU9cmVzb2x2ZTtyZWFkeVByb21pc2VSZWplY3Q9cmVqZWN0fSl9Zm9yKGNvbnN0IHByb3Agb2YgT2JqZWN0LmtleXMoTW9kdWxlKSl7aWYoIShwcm9wIGluIG1vZHVsZUFyZykpe09iamVjdC5kZWZpbmVQcm9wZXJ0eShtb2R1bGVBcmcscHJvcCx7Y29uZmlndXJhYmxlOnRydWUsZ2V0KCl7YWJvcnQoYEFjY2VzcyB0byBtb2R1bGUgcHJvcGVydHkgKCcke3Byb3B9JykgaXMgbm8gbG9uZ2VyIHBvc3NpYmxlIHZpYSB0aGUgbW9kdWxlIGNvbnN0cnVjdG9yIGFyZ3VtZW50OyBJbnN0ZWFkLCB1c2UgdGhlIHJlc3VsdCBvZiB0aGUgbW9kdWxlIGNvbnN0cnVjdG9yLmApfX0pfX0KO3JldHVybiBtb2R1bGVSdG59ZXhwb3J0IGRlZmF1bHQgY3BTYXRNb2R1bGU7dmFyIGlzUHRocmVhZD1nbG9iYWxUaGlzLnNlbGY/Lm5hbWU/LnN0YXJ0c1dpdGgoImVtLXB0aHJlYWQiKTtpc1B0aHJlYWQmJmNwU2F0TW9kdWxlKCk7Cg==", import.meta.url),
      /* @vite-ignore */
      { type: "module", name: "em-pthread-" + I.nextWorkerID }
    );
    l.workerID = I.nextWorkerID++, I.unusedWorkers.push(l);
  }, getNewWorker() {
    if (I.unusedWorkers.length == 0) {
      Y("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.");
      return;
    }
    return I.unusedWorkers.pop();
  } }, MZ = [], Xd = (l) => MZ.push(l), u = {}, rd = (l, Z, c) => {
    l = l.replace(/p/g, "i"), n(l in u, `bad function pointer type - sig is not in dynCalls: '${l}'`), c?.length ? n(c.length === l.length - 1) : n(l.length == 1);
    var d = u[l];
    return d(Z, ...c);
  }, yd = (l, Z, c = [], d = !1) => {
    n(Z, "null function pointer in dynCall"), n(!d, "async dynCall is not supported in this mode");
    var b = rd(l, Z, c);
    function m(t) {
      return t;
    }
    return b;
  };
  function Gd(l) {
    var Z = (a(), R)[l + 52 >> 2], c = (a(), R)[l + 56 >> 2], d = Z - c;
    n(Z != 0), n(d != 0), n(Z > d, "stackHigh must be higher then stackLow"), Lc(Z, d), lZ(Z), IZ();
  }
  var jZ = (l, Z) => {
    Vl = 0, eZ = 0;
    var c = ((b) => jc(l, b))(Z);
    pl();
    function d(b) {
      if (fl()) {
        nl = b;
        return;
      }
      oZ(b);
    }
    d(c);
  };
  jZ.isAsync = !0;
  var eZ = !0, hd = (l) => I.tlsInitFunctions.push(l), ll = (l) => {
    ll.shown ||= {}, ll.shown[l] || (ll.shown[l] = 1, Y(l));
  }, L, PZ = globalThis.TextDecoder && new TextDecoder(), _Z = (l, Z, c, d) => {
    var b = Z + c;
    if (d) return b;
    for (; l[Z] && !(Z >= b); ) ++Z;
    return Z;
  }, Xl = (l, Z = 0, c, d) => {
    var b = _Z(l, Z, c, d);
    if (b - Z > 16 && l.buffer && PZ)
      return PZ.decode(l.buffer instanceof ArrayBuffer ? l.subarray(Z, b) : l.slice(Z, b));
    for (var m = ""; Z < b; ) {
      var t = l[Z++];
      if (!(t & 128)) {
        m += String.fromCharCode(t);
        continue;
      }
      var i = l[Z++] & 63;
      if ((t & 224) == 192) {
        m += String.fromCharCode((t & 31) << 6 | i);
        continue;
      }
      var W = l[Z++] & 63;
      if ((t & 240) == 224 ? t = (t & 15) << 12 | i << 6 | W : ((t & 248) != 240 && ll("Invalid UTF-8 leading byte " + ml(t) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), t = (t & 7) << 18 | i << 12 | W << 6 | l[Z++] & 63), t < 65536)
        m += String.fromCharCode(t);
      else {
        var X = t - 65536;
        m += String.fromCharCode(55296 | X >> 10, 56320 | X & 1023);
      }
    }
    return m;
  }, _ = (l, Z, c) => (n(typeof l == "number", `UTF8ToString expects a number (got ${typeof l})`), l ? Xl((a(), w), l, Z, c) : ""), Rd = (l, Z, c, d) => v(`Assertion failed: ${_(l)}, at: ` + [Z ? _(Z) : "unknown filename", c, d ? _(d) : "unknown function"]);
  class od {
    constructor(Z) {
      this.excPtr = Z, this.ptr = Z - 24;
    }
    set_type(Z) {
      (a(), R)[this.ptr + 4 >> 2] = Z;
    }
    get_type() {
      return (a(), R)[this.ptr + 4 >> 2];
    }
    set_destructor(Z) {
      (a(), R)[this.ptr + 8 >> 2] = Z;
    }
    get_destructor() {
      return (a(), R)[this.ptr + 8 >> 2];
    }
    set_caught(Z) {
      Z = Z ? 1 : 0, (a(), S)[this.ptr + 12] = Z;
    }
    get_caught() {
      return (a(), S)[this.ptr + 12] != 0;
    }
    set_rethrown(Z) {
      Z = Z ? 1 : 0, (a(), S)[this.ptr + 13] = Z;
    }
    get_rethrown() {
      return (a(), S)[this.ptr + 13] != 0;
    }
    init(Z, c) {
      this.set_adjusted_ptr(0), this.set_type(Z), this.set_destructor(c);
    }
    set_adjusted_ptr(Z) {
      (a(), R)[this.ptr + 16 >> 2] = Z;
    }
    get_adjusted_ptr() {
      return (a(), R)[this.ptr + 16 >> 2];
    }
  }
  var sd = (l, Z, c) => {
    var d = new od(l);
    d.init(Z, c), n(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
  };
  function OZ(l, Z, c, d) {
    return p ? z(2, 0, 1, l, Z, c, d) : DZ(l, Z, c, d);
  }
  var ud = () => !!globalThis.SharedArrayBuffer, DZ = (l, Z, c, d) => {
    if (!ud())
      return Ol("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
    var b = [], m = 0;
    if (p && (b.length === 0 || m))
      return OZ(l, Z, c, d);
    var t = { startRoutine: c, pthread_ptr: l, arg: d, transferList: b };
    return p ? (t.cmd = "spawnThread", postMessage(t, b), 0) : QZ(t);
  }, gl = () => {
    n(H.varargs != null);
    var l = (a(), f)[+H.varargs >> 2];
    return H.varargs += 4, l;
  }, rl = gl, U = { isAbs: (l) => l.charAt(0) === "/", splitPath: (l) => {
    var Z = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
    return Z.exec(l).slice(1);
  }, normalizeArray: (l, Z) => {
    for (var c = 0, d = l.length - 1; d >= 0; d--) {
      var b = l[d];
      b === "." ? l.splice(d, 1) : b === ".." ? (l.splice(d, 1), c++) : c && (l.splice(d, 1), c--);
    }
    if (Z)
      for (; c; c--)
        l.unshift("..");
    return l;
  }, normalize: (l) => {
    var Z = U.isAbs(l), c = l.slice(-1) === "/";
    return l = U.normalizeArray(l.split("/").filter((d) => !!d), !Z).join("/"), !l && !Z && (l = "."), l && c && (l += "/"), (Z ? "/" : "") + l;
  }, dirname: (l) => {
    var Z = U.splitPath(l), c = Z[0], d = Z[1];
    return !c && !d ? "." : (d && (d = d.slice(0, -1)), c + d);
  }, basename: (l) => l && l.match(/([^\/]+|\/)\/*$/)[1], join: (...l) => U.normalize(l.join("/")), join2: (l, Z) => U.normalize(l + "/" + Z) }, pd = () => (l) => l.set(crypto.getRandomValues(new Uint8Array(l.byteLength))), bZ = (l) => {
    (bZ = pd())(l);
  }, yl = { resolve: (...l) => {
    for (var Z = "", c = !1, d = l.length - 1; d >= -1 && !c; d--) {
      var b = d >= 0 ? l[d] : e.cwd();
      if (typeof b != "string")
        throw new TypeError("Arguments to path.resolve must be strings");
      if (!b)
        return "";
      Z = b + "/" + Z, c = U.isAbs(b);
    }
    return Z = U.normalizeArray(Z.split("/").filter((m) => !!m), !c).join("/"), (c ? "/" : "") + Z || ".";
  }, relative: (l, Z) => {
    l = yl.resolve(l).slice(1), Z = yl.resolve(Z).slice(1);
    function c(X) {
      for (var h = 0; h < X.length && X[h] === ""; h++)
        ;
      for (var s = X.length - 1; s >= 0 && X[s] === ""; s--)
        ;
      return h > s ? [] : X.slice(h, s - h + 1);
    }
    for (var d = c(l.split("/")), b = c(Z.split("/")), m = Math.min(d.length, b.length), t = m, i = 0; i < m; i++)
      if (d[i] !== b[i]) {
        t = i;
        break;
      }
    for (var W = [], i = t; i < d.length; i++)
      W.push("..");
    return W = W.concat(b.slice(t)), W.join("/");
  } }, mZ = [], tl = (l) => {
    for (var Z = 0, c = 0; c < l.length; ++c) {
      var d = l.charCodeAt(c);
      d <= 127 ? Z++ : d <= 2047 ? Z += 2 : d >= 55296 && d <= 57343 ? (Z += 4, ++c) : Z += 3;
    }
    return Z;
  }, AZ = (l, Z, c, d) => {
    if (n(typeof l == "string", `stringToUTF8Array expects a string (got ${typeof l})`), !(d > 0)) return 0;
    for (var b = c, m = c + d - 1, t = 0; t < l.length; ++t) {
      var i = l.codePointAt(t);
      if (i <= 127) {
        if (c >= m) break;
        Z[c++] = i;
      } else if (i <= 2047) {
        if (c + 1 >= m) break;
        Z[c++] = 192 | i >> 6, Z[c++] = 128 | i & 63;
      } else if (i <= 65535) {
        if (c + 2 >= m) break;
        Z[c++] = 224 | i >> 12, Z[c++] = 128 | i >> 6 & 63, Z[c++] = 128 | i & 63;
      } else {
        if (c + 3 >= m) break;
        i > 1114111 && ll("Invalid Unicode code point " + ml(i) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), Z[c++] = 240 | i >> 18, Z[c++] = 128 | i >> 12 & 63, Z[c++] = 128 | i >> 6 & 63, Z[c++] = 128 | i & 63, t++;
      }
    }
    return Z[c] = 0, c - b;
  }, tZ = (l, Z, c) => {
    var d = tl(l) + 1, b = new Array(d), m = AZ(l, b, 0, b.length);
    return b.length = m, b;
  }, Fd = () => {
    if (!mZ.length) {
      var l = null;
      if (globalThis.window?.prompt && (l = window.prompt("Input: "), l !== null && (l += `
`)), !l)
        return null;
      mZ = tZ(l);
    }
    return mZ.shift();
  }, Zl = { ttys: [], init() {
  }, shutdown() {
  }, register(l, Z) {
    Zl.ttys[l] = { input: [], output: [], ops: Z }, e.registerDevice(l, Zl.stream_ops);
  }, stream_ops: { open(l) {
    var Z = Zl.ttys[l.node.rdev];
    if (!Z)
      throw new e.ErrnoError(43);
    l.tty = Z, l.seekable = !1;
  }, close(l) {
    l.tty.ops.fsync(l.tty);
  }, fsync(l) {
    l.tty.ops.fsync(l.tty);
  }, read(l, Z, c, d, b) {
    if (!l.tty || !l.tty.ops.get_char)
      throw new e.ErrnoError(60);
    for (var m = 0, t = 0; t < d; t++) {
      var i;
      try {
        i = l.tty.ops.get_char(l.tty);
      } catch {
        throw new e.ErrnoError(29);
      }
      if (i === void 0 && m === 0)
        throw new e.ErrnoError(6);
      if (i == null) break;
      m++, Z[c + t] = i;
    }
    return m && (l.node.atime = Date.now()), m;
  }, write(l, Z, c, d, b) {
    if (!l.tty || !l.tty.ops.put_char)
      throw new e.ErrnoError(60);
    try {
      for (var m = 0; m < d; m++)
        l.tty.ops.put_char(l.tty, Z[c + m]);
    } catch {
      throw new e.ErrnoError(29);
    }
    return d && (l.node.mtime = l.node.ctime = Date.now()), m;
  } }, default_tty_ops: { get_char(l) {
    return Fd();
  }, put_char(l, Z) {
    Z === null || Z === 10 ? (q(Xl(l.output)), l.output = []) : Z != 0 && l.output.push(Z);
  }, fsync(l) {
    l.output?.length > 0 && (q(Xl(l.output)), l.output = []);
  }, ioctl_tcgets(l) {
    return { c_iflag: 25856, c_oflag: 5, c_cflag: 191, c_lflag: 35387, c_cc: [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] };
  }, ioctl_tcsets(l, Z, c) {
    return 0;
  }, ioctl_tiocgwinsz(l) {
    return [24, 80];
  } }, default_tty1_ops: { put_char(l, Z) {
    Z === null || Z === 10 ? (Y(Xl(l.output)), l.output = []) : Z != 0 && l.output.push(Z);
  }, fsync(l) {
    l.output?.length > 0 && (Y(Xl(l.output)), l.output = []);
  } } }, Yd = (l, Z) => (a(), w).fill(0, l, l + Z), $Z = (l, Z) => (n(Z, "alignment argument is required"), Math.ceil(l / Z) * Z), qZ = (l) => {
    l = $Z(l, 65536);
    var Z = Sc(65536, l);
    return Z && Yd(Z, l), Z;
  }, F = { ops_table: null, mount(l) {
    return F.createNode(null, "/", 16895, 0);
  }, createNode(l, Z, c, d) {
    if (e.isBlkdev(c) || e.isFIFO(c))
      throw new e.ErrnoError(63);
    F.ops_table ||= { dir: { node: { getattr: F.node_ops.getattr, setattr: F.node_ops.setattr, lookup: F.node_ops.lookup, mknod: F.node_ops.mknod, rename: F.node_ops.rename, unlink: F.node_ops.unlink, rmdir: F.node_ops.rmdir, readdir: F.node_ops.readdir, symlink: F.node_ops.symlink }, stream: { llseek: F.stream_ops.llseek } }, file: { node: { getattr: F.node_ops.getattr, setattr: F.node_ops.setattr }, stream: { llseek: F.stream_ops.llseek, read: F.stream_ops.read, write: F.stream_ops.write, mmap: F.stream_ops.mmap, msync: F.stream_ops.msync } }, link: { node: { getattr: F.node_ops.getattr, setattr: F.node_ops.setattr, readlink: F.node_ops.readlink }, stream: {} }, chrdev: { node: { getattr: F.node_ops.getattr, setattr: F.node_ops.setattr }, stream: e.chrdev_stream_ops } };
    var b = e.createNode(l, Z, c, d);
    return e.isDir(b.mode) ? (b.node_ops = F.ops_table.dir.node, b.stream_ops = F.ops_table.dir.stream, b.contents = {}) : e.isFile(b.mode) ? (b.node_ops = F.ops_table.file.node, b.stream_ops = F.ops_table.file.stream, b.usedBytes = 0, b.contents = null) : e.isLink(b.mode) ? (b.node_ops = F.ops_table.link.node, b.stream_ops = F.ops_table.link.stream) : e.isChrdev(b.mode) && (b.node_ops = F.ops_table.chrdev.node, b.stream_ops = F.ops_table.chrdev.stream), b.atime = b.mtime = b.ctime = Date.now(), l && (l.contents[Z] = b, l.atime = l.mtime = l.ctime = b.atime), b;
  }, getFileDataAsTypedArray(l) {
    return l.contents ? l.contents.subarray ? l.contents.subarray(0, l.usedBytes) : new Uint8Array(l.contents) : new Uint8Array(0);
  }, expandFileStorage(l, Z) {
    var c = l.contents ? l.contents.length : 0;
    if (!(c >= Z)) {
      var d = 1024 * 1024;
      Z = Math.max(Z, c * (c < d ? 2 : 1.125) >>> 0), c != 0 && (Z = Math.max(Z, 256));
      var b = l.contents;
      l.contents = new Uint8Array(Z), l.usedBytes > 0 && l.contents.set(b.subarray(0, l.usedBytes), 0);
    }
  }, resizeFileStorage(l, Z) {
    if (l.usedBytes != Z)
      if (Z == 0)
        l.contents = null, l.usedBytes = 0;
      else {
        var c = l.contents;
        l.contents = new Uint8Array(Z), c && l.contents.set(c.subarray(0, Math.min(Z, l.usedBytes))), l.usedBytes = Z;
      }
  }, node_ops: { getattr(l) {
    var Z = {};
    return Z.dev = e.isChrdev(l.mode) ? l.id : 1, Z.ino = l.id, Z.mode = l.mode, Z.nlink = 1, Z.uid = 0, Z.gid = 0, Z.rdev = l.rdev, e.isDir(l.mode) ? Z.size = 4096 : e.isFile(l.mode) ? Z.size = l.usedBytes : e.isLink(l.mode) ? Z.size = l.link.length : Z.size = 0, Z.atime = new Date(l.atime), Z.mtime = new Date(l.mtime), Z.ctime = new Date(l.ctime), Z.blksize = 4096, Z.blocks = Math.ceil(Z.size / Z.blksize), Z;
  }, setattr(l, Z) {
    for (const c of ["mode", "atime", "mtime", "ctime"])
      Z[c] != null && (l[c] = Z[c]);
    Z.size !== void 0 && F.resizeFileStorage(l, Z.size);
  }, lookup(l, Z) {
    throw new e.ErrnoError(44);
  }, mknod(l, Z, c, d) {
    return F.createNode(l, Z, c, d);
  }, rename(l, Z, c) {
    var d;
    try {
      d = e.lookupNode(Z, c);
    } catch {
    }
    if (d) {
      if (e.isDir(l.mode))
        for (var b in d.contents)
          throw new e.ErrnoError(55);
      e.hashRemoveNode(d);
    }
    delete l.parent.contents[l.name], Z.contents[c] = l, l.name = c, Z.ctime = Z.mtime = l.parent.ctime = l.parent.mtime = Date.now();
  }, unlink(l, Z) {
    delete l.contents[Z], l.ctime = l.mtime = Date.now();
  }, rmdir(l, Z) {
    var c = e.lookupNode(l, Z);
    for (var d in c.contents)
      throw new e.ErrnoError(55);
    delete l.contents[Z], l.ctime = l.mtime = Date.now();
  }, readdir(l) {
    return [".", "..", ...Object.keys(l.contents)];
  }, symlink(l, Z, c) {
    var d = F.createNode(l, Z, 41471, 0);
    return d.link = c, d;
  }, readlink(l) {
    if (!e.isLink(l.mode))
      throw new e.ErrnoError(28);
    return l.link;
  } }, stream_ops: { read(l, Z, c, d, b) {
    var m = l.node.contents;
    if (b >= l.node.usedBytes) return 0;
    var t = Math.min(l.node.usedBytes - b, d);
    if (n(t >= 0), t > 8 && m.subarray)
      Z.set(m.subarray(b, b + t), c);
    else
      for (var i = 0; i < t; i++) Z[c + i] = m[b + i];
    return t;
  }, write(l, Z, c, d, b, m) {
    if (n(!(Z instanceof ArrayBuffer)), Z.buffer === (a(), S).buffer && (m = !1), !d) return 0;
    var t = l.node;
    if (t.mtime = t.ctime = Date.now(), Z.subarray && (!t.contents || t.contents.subarray)) {
      if (m)
        return n(b === 0, "canOwn must imply no weird position inside the file"), t.contents = Z.subarray(c, c + d), t.usedBytes = d, d;
      if (t.usedBytes === 0 && b === 0)
        return t.contents = Z.slice(c, c + d), t.usedBytes = d, d;
      if (b + d <= t.usedBytes)
        return t.contents.set(Z.subarray(c, c + d), b), d;
    }
    if (F.expandFileStorage(t, b + d), t.contents.subarray && Z.subarray)
      t.contents.set(Z.subarray(c, c + d), b);
    else
      for (var i = 0; i < d; i++)
        t.contents[b + i] = Z[c + i];
    return t.usedBytes = Math.max(t.usedBytes, b + d), d;
  }, llseek(l, Z, c) {
    var d = Z;
    if (c === 1 ? d += l.position : c === 2 && e.isFile(l.node.mode) && (d += l.node.usedBytes), d < 0)
      throw new e.ErrnoError(28);
    return d;
  }, mmap(l, Z, c, d, b) {
    if (!e.isFile(l.node.mode))
      throw new e.ErrnoError(43);
    var m, t, i = l.node.contents;
    if (!(b & 2) && i && i.buffer === (a(), S).buffer)
      t = !1, m = i.byteOffset;
    else {
      if (t = !0, m = qZ(Z), !m)
        throw new e.ErrnoError(48);
      i && ((c > 0 || c + Z < i.length) && (i.subarray ? i = i.subarray(c, c + Z) : i = Array.prototype.slice.call(i, c, c + Z)), (a(), S).set(i, m));
    }
    return { ptr: m, allocated: t };
  }, msync(l, Z, c, d, b) {
    return F.stream_ops.write(l, Z, 0, d, c, !1), 0;
  } } }, Id = (l) => {
    var Z = { r: 0, "r+": 2, w: 577, "w+": 578, a: 1089, "a+": 1090 }, c = Z[l];
    if (typeof c > "u")
      throw new Error(`Unknown file open mode: ${l}`);
    return c;
  }, iZ = (l, Z) => {
    var c = 0;
    return l && (c |= 365), Z && (c |= 146), c;
  }, Jd = (l) => _(zc(l)), lc = { EPERM: 63, ENOENT: 44, ESRCH: 71, EINTR: 27, EIO: 29, ENXIO: 60, E2BIG: 1, ENOEXEC: 45, EBADF: 8, ECHILD: 12, EAGAIN: 6, EWOULDBLOCK: 6, ENOMEM: 48, EACCES: 2, EFAULT: 21, ENOTBLK: 105, EBUSY: 10, EEXIST: 20, EXDEV: 75, ENODEV: 43, ENOTDIR: 54, EISDIR: 31, EINVAL: 28, ENFILE: 41, EMFILE: 33, ENOTTY: 59, ETXTBSY: 74, EFBIG: 22, ENOSPC: 51, ESPIPE: 70, EROFS: 69, EMLINK: 34, EPIPE: 64, EDOM: 18, ERANGE: 68, ENOMSG: 49, EIDRM: 24, ECHRNG: 106, EL2NSYNC: 156, EL3HLT: 107, EL3RST: 108, ELNRNG: 109, EUNATCH: 110, ENOCSI: 111, EL2HLT: 112, EDEADLK: 16, ENOLCK: 46, EBADE: 113, EBADR: 114, EXFULL: 115, ENOANO: 104, EBADRQC: 103, EBADSLT: 102, EDEADLOCK: 16, EBFONT: 101, ENOSTR: 100, ENODATA: 116, ETIME: 117, ENOSR: 118, ENONET: 119, ENOPKG: 120, EREMOTE: 121, ENOLINK: 47, EADV: 122, ESRMNT: 123, ECOMM: 124, EPROTO: 65, EMULTIHOP: 36, EDOTDOT: 125, EBADMSG: 9, ENOTUNIQ: 126, EBADFD: 127, EREMCHG: 128, ELIBACC: 129, ELIBBAD: 130, ELIBSCN: 131, ELIBMAX: 132, ELIBEXEC: 133, ENOSYS: 52, ENOTEMPTY: 55, ENAMETOOLONG: 37, ELOOP: 32, EOPNOTSUPP: 138, EPFNOSUPPORT: 139, ECONNRESET: 15, ENOBUFS: 42, EAFNOSUPPORT: 5, EPROTOTYPE: 67, ENOTSOCK: 57, ENOPROTOOPT: 50, ESHUTDOWN: 140, ECONNREFUSED: 14, EADDRINUSE: 3, ECONNABORTED: 13, ENETUNREACH: 40, ENETDOWN: 38, ETIMEDOUT: 73, EHOSTDOWN: 142, EHOSTUNREACH: 23, EINPROGRESS: 26, EALREADY: 7, EDESTADDRREQ: 17, EMSGSIZE: 35, EPROTONOSUPPORT: 66, ESOCKTNOSUPPORT: 137, EADDRNOTAVAIL: 4, ENETRESET: 39, EISCONN: 30, ENOTCONN: 53, ETOOMANYREFS: 141, EUSERS: 136, EDQUOT: 19, ESTALE: 72, ENOTSUP: 138, ENOMEDIUM: 148, EILSEQ: 25, EOVERFLOW: 61, ECANCELED: 11, ENOTRECOVERABLE: 56, EOWNERDEAD: 62, ESTRPIPE: 135 }, Nd = async (l) => {
    var Z = await _l(l);
    return n(Z, `Loading data file "${l}" failed (no arrayBuffer).`), new Uint8Array(Z);
  }, kd = (...l) => e.createDataFile(...l), vd = (l) => {
    for (var Z = l; ; ) {
      if (!al[l]) return l;
      l = Z + Math.random();
    }
  }, Zc = [], Ud = async (l, Z) => {
    typeof Browser < "u" && Browser.init();
    for (var c of Zc)
      if (c.canHandle(Z))
        return n(c.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), c.handle(l, Z);
    return l;
  }, cc = async (l, Z, c, d, b, m, t, i) => {
    var W = Z ? yl.resolve(U.join2(l, Z)) : l, X = vd(`cp ${W}`);
    CZ(X);
    try {
      var h = c;
      typeof c == "string" && (h = await Nd(c)), h = await Ud(h, W), i?.(), m || kd(l, Z, h, d, b, t);
    } finally {
      wZ(X);
    }
  }, Hd = (l, Z, c, d, b, m, t, i, W, X) => {
    cc(l, Z, c, d, b, i, W, X).then(m).catch(t);
  }, e = { root: null, mounts: [], devices: {}, streams: [], nextInode: 1, nameTable: null, currentPath: "/", initialized: !1, ignorePermissions: !0, filesystems: null, syncFSRequests: 0, readFiles: {}, ErrnoError: class extends Error {
    name = "ErrnoError";
    constructor(l) {
      super(Wl ? Jd(l) : ""), this.errno = l;
      for (var Z in lc)
        if (lc[Z] === l) {
          this.code = Z;
          break;
        }
    }
  }, FSStream: class {
    shared = {};
    get object() {
      return this.node;
    }
    set object(l) {
      this.node = l;
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
    set flags(l) {
      this.shared.flags = l;
    }
    get position() {
      return this.shared.position;
    }
    set position(l) {
      this.shared.position = l;
    }
  }, FSNode: class {
    node_ops = {};
    stream_ops = {};
    readMode = 365;
    writeMode = 146;
    mounted = null;
    constructor(l, Z, c, d) {
      l || (l = this), this.parent = l, this.mount = l.mount, this.id = e.nextInode++, this.name = Z, this.mode = c, this.rdev = d, this.atime = this.mtime = this.ctime = Date.now();
    }
    get read() {
      return (this.mode & this.readMode) === this.readMode;
    }
    set read(l) {
      l ? this.mode |= this.readMode : this.mode &= ~this.readMode;
    }
    get write() {
      return (this.mode & this.writeMode) === this.writeMode;
    }
    set write(l) {
      l ? this.mode |= this.writeMode : this.mode &= ~this.writeMode;
    }
    get isFolder() {
      return e.isDir(this.mode);
    }
    get isDevice() {
      return e.isChrdev(this.mode);
    }
  }, lookupPath(l, Z = {}) {
    if (!l)
      throw new e.ErrnoError(44);
    Z.follow_mount ??= !0, U.isAbs(l) || (l = e.cwd() + "/" + l);
    l: for (var c = 0; c < 40; c++) {
      for (var d = l.split("/").filter((X) => !!X), b = e.root, m = "/", t = 0; t < d.length; t++) {
        var i = t === d.length - 1;
        if (i && Z.parent)
          break;
        if (d[t] !== ".") {
          if (d[t] === "..") {
            if (m = U.dirname(m), e.isRoot(b)) {
              l = m + "/" + d.slice(t + 1).join("/"), c--;
              continue l;
            } else
              b = b.parent;
            continue;
          }
          m = U.join2(m, d[t]);
          try {
            b = e.lookupNode(b, d[t]);
          } catch (X) {
            if (X?.errno === 44 && i && Z.noent_okay)
              return { path: m };
            throw X;
          }
          if (e.isMountpoint(b) && (!i || Z.follow_mount) && (b = b.mounted.root), e.isLink(b.mode) && (!i || Z.follow)) {
            if (!b.node_ops.readlink)
              throw new e.ErrnoError(52);
            var W = b.node_ops.readlink(b);
            U.isAbs(W) || (W = U.dirname(m) + "/" + W), l = W + "/" + d.slice(t + 1).join("/");
            continue l;
          }
        }
      }
      return { path: m, node: b };
    }
    throw new e.ErrnoError(32);
  }, getPath(l) {
    for (var Z; ; ) {
      if (e.isRoot(l)) {
        var c = l.mount.mountpoint;
        return Z ? c[c.length - 1] !== "/" ? `${c}/${Z}` : c + Z : c;
      }
      Z = Z ? `${l.name}/${Z}` : l.name, l = l.parent;
    }
  }, hashName(l, Z) {
    for (var c = 0, d = 0; d < Z.length; d++)
      c = (c << 5) - c + Z.charCodeAt(d) | 0;
    return (l + c >>> 0) % e.nameTable.length;
  }, hashAddNode(l) {
    var Z = e.hashName(l.parent.id, l.name);
    l.name_next = e.nameTable[Z], e.nameTable[Z] = l;
  }, hashRemoveNode(l) {
    var Z = e.hashName(l.parent.id, l.name);
    if (e.nameTable[Z] === l)
      e.nameTable[Z] = l.name_next;
    else
      for (var c = e.nameTable[Z]; c; ) {
        if (c.name_next === l) {
          c.name_next = l.name_next;
          break;
        }
        c = c.name_next;
      }
  }, lookupNode(l, Z) {
    var c = e.mayLookup(l);
    if (c)
      throw new e.ErrnoError(c);
    for (var d = e.hashName(l.id, Z), b = e.nameTable[d]; b; b = b.name_next) {
      var m = b.name;
      if (b.parent.id === l.id && m === Z)
        return b;
    }
    return e.lookup(l, Z);
  }, createNode(l, Z, c, d) {
    n(typeof l == "object");
    var b = new e.FSNode(l, Z, c, d);
    return e.hashAddNode(b), b;
  }, destroyNode(l) {
    e.hashRemoveNode(l);
  }, isRoot(l) {
    return l === l.parent;
  }, isMountpoint(l) {
    return !!l.mounted;
  }, isFile(l) {
    return (l & 61440) === 32768;
  }, isDir(l) {
    return (l & 61440) === 16384;
  }, isLink(l) {
    return (l & 61440) === 40960;
  }, isChrdev(l) {
    return (l & 61440) === 8192;
  }, isBlkdev(l) {
    return (l & 61440) === 24576;
  }, isFIFO(l) {
    return (l & 61440) === 4096;
  }, isSocket(l) {
    return (l & 49152) === 49152;
  }, flagsToPermissionString(l) {
    var Z = ["r", "w", "rw"][l & 3];
    return l & 512 && (Z += "w"), Z;
  }, nodePermissions(l, Z) {
    return e.ignorePermissions ? 0 : Z.includes("r") && !(l.mode & 292) || Z.includes("w") && !(l.mode & 146) || Z.includes("x") && !(l.mode & 73) ? 2 : 0;
  }, mayLookup(l) {
    if (!e.isDir(l.mode)) return 54;
    var Z = e.nodePermissions(l, "x");
    return Z || (l.node_ops.lookup ? 0 : 2);
  }, mayCreate(l, Z) {
    if (!e.isDir(l.mode))
      return 54;
    try {
      var c = e.lookupNode(l, Z);
      return 20;
    } catch {
    }
    return e.nodePermissions(l, "wx");
  }, mayDelete(l, Z, c) {
    var d;
    try {
      d = e.lookupNode(l, Z);
    } catch (m) {
      return m.errno;
    }
    var b = e.nodePermissions(l, "wx");
    if (b)
      return b;
    if (c) {
      if (!e.isDir(d.mode))
        return 54;
      if (e.isRoot(d) || e.getPath(d) === e.cwd())
        return 10;
    } else if (e.isDir(d.mode))
      return 31;
    return 0;
  }, mayOpen(l, Z) {
    return l ? e.isLink(l.mode) ? 32 : e.isDir(l.mode) && (e.flagsToPermissionString(Z) !== "r" || Z & 576) ? 31 : e.nodePermissions(l, e.flagsToPermissionString(Z)) : 44;
  }, checkOpExists(l, Z) {
    if (!l)
      throw new e.ErrnoError(Z);
    return l;
  }, MAX_OPEN_FDS: 4096, nextfd() {
    for (var l = 0; l <= e.MAX_OPEN_FDS; l++)
      if (!e.streams[l])
        return l;
    throw new e.ErrnoError(33);
  }, getStreamChecked(l) {
    var Z = e.getStream(l);
    if (!Z)
      throw new e.ErrnoError(8);
    return Z;
  }, getStream: (l) => e.streams[l], createStream(l, Z = -1) {
    return n(Z >= -1), l = Object.assign(new e.FSStream(), l), Z == -1 && (Z = e.nextfd()), l.fd = Z, e.streams[Z] = l, l;
  }, closeStream(l) {
    e.streams[l] = null;
  }, dupStream(l, Z = -1) {
    var c = e.createStream(l, Z);
    return c.stream_ops?.dup?.(c), c;
  }, doSetAttr(l, Z, c) {
    var d = l?.stream_ops.setattr, b = d ? l : Z;
    d ??= Z.node_ops.setattr, e.checkOpExists(d, 63), d(b, c);
  }, chrdev_stream_ops: { open(l) {
    var Z = e.getDevice(l.node.rdev);
    l.stream_ops = Z.stream_ops, l.stream_ops.open?.(l);
  }, llseek() {
    throw new e.ErrnoError(70);
  } }, major: (l) => l >> 8, minor: (l) => l & 255, makedev: (l, Z) => l << 8 | Z, registerDevice(l, Z) {
    e.devices[l] = { stream_ops: Z };
  }, getDevice: (l) => e.devices[l], getMounts(l) {
    for (var Z = [], c = [l]; c.length; ) {
      var d = c.pop();
      Z.push(d), c.push(...d.mounts);
    }
    return Z;
  }, syncfs(l, Z) {
    typeof l == "function" && (Z = l, l = !1), e.syncFSRequests++, e.syncFSRequests > 1 && Y(`warning: ${e.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
    var c = e.getMounts(e.root.mount), d = 0;
    function b(i) {
      return n(e.syncFSRequests > 0), e.syncFSRequests--, Z(i);
    }
    function m(i) {
      if (i)
        return m.errored ? void 0 : (m.errored = !0, b(i));
      ++d >= c.length && b(null);
    }
    for (var t of c)
      t.type.syncfs ? t.type.syncfs(t, l, m) : m(null);
  }, mount(l, Z, c) {
    if (typeof l == "string")
      throw l;
    var d = c === "/", b = !c, m;
    if (d && e.root)
      throw new e.ErrnoError(10);
    if (!d && !b) {
      var t = e.lookupPath(c, { follow_mount: !1 });
      if (c = t.path, m = t.node, e.isMountpoint(m))
        throw new e.ErrnoError(10);
      if (!e.isDir(m.mode))
        throw new e.ErrnoError(54);
    }
    var i = { type: l, opts: Z, mountpoint: c, mounts: [] }, W = l.mount(i);
    return W.mount = i, i.root = W, d ? e.root = W : m && (m.mounted = i, m.mount && m.mount.mounts.push(i)), W;
  }, unmount(l) {
    var Z = e.lookupPath(l, { follow_mount: !1 });
    if (!e.isMountpoint(Z.node))
      throw new e.ErrnoError(28);
    var c = Z.node, d = c.mounted, b = e.getMounts(d);
    for (var [m, t] of Object.entries(e.nameTable))
      for (; t; ) {
        var i = t.name_next;
        b.includes(t.mount) && e.destroyNode(t), t = i;
      }
    c.mounted = null;
    var W = c.mount.mounts.indexOf(d);
    n(W !== -1), c.mount.mounts.splice(W, 1);
  }, lookup(l, Z) {
    return l.node_ops.lookup(l, Z);
  }, mknod(l, Z, c) {
    var d = e.lookupPath(l, { parent: !0 }), b = d.node, m = U.basename(l);
    if (!m)
      throw new e.ErrnoError(28);
    if (m === "." || m === "..")
      throw new e.ErrnoError(20);
    var t = e.mayCreate(b, m);
    if (t)
      throw new e.ErrnoError(t);
    if (!b.node_ops.mknod)
      throw new e.ErrnoError(63);
    return b.node_ops.mknod(b, m, Z, c);
  }, statfs(l) {
    return e.statfsNode(e.lookupPath(l, { follow: !0 }).node);
  }, statfsStream(l) {
    return e.statfsNode(l.node);
  }, statfsNode(l) {
    var Z = { bsize: 4096, frsize: 4096, blocks: 1e6, bfree: 5e5, bavail: 5e5, files: e.nextInode, ffree: e.nextInode - 1, fsid: 42, flags: 2, namelen: 255 };
    return l.node_ops.statfs && Object.assign(Z, l.node_ops.statfs(l.mount.opts.root)), Z;
  }, create(l, Z = 438) {
    return Z &= 4095, Z |= 32768, e.mknod(l, Z, 0);
  }, mkdir(l, Z = 511) {
    return Z &= 1023, Z |= 16384, e.mknod(l, Z, 0);
  }, mkdirTree(l, Z) {
    var c = l.split("/"), d = "";
    for (var b of c)
      if (b) {
        (d || U.isAbs(l)) && (d += "/"), d += b;
        try {
          e.mkdir(d, Z);
        } catch (m) {
          if (m.errno != 20) throw m;
        }
      }
  }, mkdev(l, Z, c) {
    return typeof c > "u" && (c = Z, Z = 438), Z |= 8192, e.mknod(l, Z, c);
  }, symlink(l, Z) {
    if (!yl.resolve(l))
      throw new e.ErrnoError(44);
    var c = e.lookupPath(Z, { parent: !0 }), d = c.node;
    if (!d)
      throw new e.ErrnoError(44);
    var b = U.basename(Z), m = e.mayCreate(d, b);
    if (m)
      throw new e.ErrnoError(m);
    if (!d.node_ops.symlink)
      throw new e.ErrnoError(63);
    return d.node_ops.symlink(d, b, l);
  }, rename(l, Z) {
    var c = U.dirname(l), d = U.dirname(Z), b = U.basename(l), m = U.basename(Z), t, i, W;
    if (t = e.lookupPath(l, { parent: !0 }), i = t.node, t = e.lookupPath(Z, { parent: !0 }), W = t.node, !i || !W) throw new e.ErrnoError(44);
    if (i.mount !== W.mount)
      throw new e.ErrnoError(75);
    var X = e.lookupNode(i, b), h = yl.relative(l, d);
    if (h.charAt(0) !== ".")
      throw new e.ErrnoError(28);
    if (h = yl.relative(Z, c), h.charAt(0) !== ".")
      throw new e.ErrnoError(55);
    var s;
    try {
      s = e.lookupNode(W, m);
    } catch {
    }
    if (X !== s) {
      var G = e.isDir(X.mode), o = e.mayDelete(i, b, G);
      if (o)
        throw new e.ErrnoError(o);
      if (o = s ? e.mayDelete(W, m, G) : e.mayCreate(W, m), o)
        throw new e.ErrnoError(o);
      if (!i.node_ops.rename)
        throw new e.ErrnoError(63);
      if (e.isMountpoint(X) || s && e.isMountpoint(s))
        throw new e.ErrnoError(10);
      if (W !== i && (o = e.nodePermissions(i, "w"), o))
        throw new e.ErrnoError(o);
      e.hashRemoveNode(X);
      try {
        i.node_ops.rename(X, W, m), X.parent = W;
      } catch (J) {
        throw J;
      } finally {
        e.hashAddNode(X);
      }
    }
  }, rmdir(l) {
    var Z = e.lookupPath(l, { parent: !0 }), c = Z.node, d = U.basename(l), b = e.lookupNode(c, d), m = e.mayDelete(c, d, !0);
    if (m)
      throw new e.ErrnoError(m);
    if (!c.node_ops.rmdir)
      throw new e.ErrnoError(63);
    if (e.isMountpoint(b))
      throw new e.ErrnoError(10);
    c.node_ops.rmdir(c, d), e.destroyNode(b);
  }, readdir(l) {
    var Z = e.lookupPath(l, { follow: !0 }), c = Z.node, d = e.checkOpExists(c.node_ops.readdir, 54);
    return d(c);
  }, unlink(l) {
    var Z = e.lookupPath(l, { parent: !0 }), c = Z.node;
    if (!c)
      throw new e.ErrnoError(44);
    var d = U.basename(l), b = e.lookupNode(c, d), m = e.mayDelete(c, d, !1);
    if (m)
      throw new e.ErrnoError(m);
    if (!c.node_ops.unlink)
      throw new e.ErrnoError(63);
    if (e.isMountpoint(b))
      throw new e.ErrnoError(10);
    c.node_ops.unlink(c, d), e.destroyNode(b);
  }, readlink(l) {
    var Z = e.lookupPath(l), c = Z.node;
    if (!c)
      throw new e.ErrnoError(44);
    if (!c.node_ops.readlink)
      throw new e.ErrnoError(28);
    return c.node_ops.readlink(c);
  }, stat(l, Z) {
    var c = e.lookupPath(l, { follow: !Z }), d = c.node, b = e.checkOpExists(d.node_ops.getattr, 63);
    return b(d);
  }, fstat(l) {
    var Z = e.getStreamChecked(l), c = Z.node, d = Z.stream_ops.getattr, b = d ? Z : c;
    return d ??= c.node_ops.getattr, e.checkOpExists(d, 63), d(b);
  }, lstat(l) {
    return e.stat(l, !0);
  }, doChmod(l, Z, c, d) {
    e.doSetAttr(l, Z, { mode: c & 4095 | Z.mode & -4096, ctime: Date.now(), dontFollow: d });
  }, chmod(l, Z, c) {
    var d;
    if (typeof l == "string") {
      var b = e.lookupPath(l, { follow: !c });
      d = b.node;
    } else
      d = l;
    e.doChmod(null, d, Z, c);
  }, lchmod(l, Z) {
    e.chmod(l, Z, !0);
  }, fchmod(l, Z) {
    var c = e.getStreamChecked(l);
    e.doChmod(c, c.node, Z, !1);
  }, doChown(l, Z, c) {
    e.doSetAttr(l, Z, { timestamp: Date.now(), dontFollow: c });
  }, chown(l, Z, c, d) {
    var b;
    if (typeof l == "string") {
      var m = e.lookupPath(l, { follow: !d });
      b = m.node;
    } else
      b = l;
    e.doChown(null, b, d);
  }, lchown(l, Z, c) {
    e.chown(l, Z, c, !0);
  }, fchown(l, Z, c) {
    var d = e.getStreamChecked(l);
    e.doChown(d, d.node, !1);
  }, doTruncate(l, Z, c) {
    if (e.isDir(Z.mode))
      throw new e.ErrnoError(31);
    if (!e.isFile(Z.mode))
      throw new e.ErrnoError(28);
    var d = e.nodePermissions(Z, "w");
    if (d)
      throw new e.ErrnoError(d);
    e.doSetAttr(l, Z, { size: c, timestamp: Date.now() });
  }, truncate(l, Z) {
    if (Z < 0)
      throw new e.ErrnoError(28);
    var c;
    if (typeof l == "string") {
      var d = e.lookupPath(l, { follow: !0 });
      c = d.node;
    } else
      c = l;
    e.doTruncate(null, c, Z);
  }, ftruncate(l, Z) {
    var c = e.getStreamChecked(l);
    if (Z < 0 || (c.flags & 2097155) === 0)
      throw new e.ErrnoError(28);
    e.doTruncate(c, c.node, Z);
  }, utime(l, Z, c) {
    var d = e.lookupPath(l, { follow: !0 }), b = d.node, m = e.checkOpExists(b.node_ops.setattr, 63);
    m(b, { atime: Z, mtime: c });
  }, open(l, Z, c = 438) {
    if (l === "")
      throw new e.ErrnoError(44);
    Z = typeof Z == "string" ? Id(Z) : Z, Z & 64 ? c = c & 4095 | 32768 : c = 0;
    var d, b;
    if (typeof l == "object")
      d = l;
    else {
      b = l.endsWith("/");
      var m = e.lookupPath(l, { follow: !(Z & 131072), noent_okay: !0 });
      d = m.node, l = m.path;
    }
    var t = !1;
    if (Z & 64)
      if (d) {
        if (Z & 128)
          throw new e.ErrnoError(20);
      } else {
        if (b)
          throw new e.ErrnoError(31);
        d = e.mknod(l, c | 511, 0), t = !0;
      }
    if (!d)
      throw new e.ErrnoError(44);
    if (e.isChrdev(d.mode) && (Z &= -513), Z & 65536 && !e.isDir(d.mode))
      throw new e.ErrnoError(54);
    if (!t) {
      var i = e.mayOpen(d, Z);
      if (i)
        throw new e.ErrnoError(i);
    }
    Z & 512 && !t && e.truncate(d, 0), Z &= -131713;
    var W = e.createStream({ node: d, path: e.getPath(d), flags: Z, seekable: !0, position: 0, stream_ops: d.stream_ops, ungotten: [], error: !1 });
    return W.stream_ops.open && W.stream_ops.open(W), t && e.chmod(d, c & 511), V.logReadFiles && !(Z & 1) && (l in e.readFiles || (e.readFiles[l] = 1)), W;
  }, close(l) {
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    l.getdents && (l.getdents = null);
    try {
      l.stream_ops.close && l.stream_ops.close(l);
    } catch (Z) {
      throw Z;
    } finally {
      e.closeStream(l.fd);
    }
    l.fd = null;
  }, isClosed(l) {
    return l.fd === null;
  }, llseek(l, Z, c) {
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    if (!l.seekable || !l.stream_ops.llseek)
      throw new e.ErrnoError(70);
    if (c != 0 && c != 1 && c != 2)
      throw new e.ErrnoError(28);
    return l.position = l.stream_ops.llseek(l, Z, c), l.ungotten = [], l.position;
  }, read(l, Z, c, d, b) {
    if (n(c >= 0), d < 0 || b < 0)
      throw new e.ErrnoError(28);
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    if ((l.flags & 2097155) === 1)
      throw new e.ErrnoError(8);
    if (e.isDir(l.node.mode))
      throw new e.ErrnoError(31);
    if (!l.stream_ops.read)
      throw new e.ErrnoError(28);
    var m = typeof b < "u";
    if (!m)
      b = l.position;
    else if (!l.seekable)
      throw new e.ErrnoError(70);
    var t = l.stream_ops.read(l, Z, c, d, b);
    return m || (l.position += t), t;
  }, write(l, Z, c, d, b, m) {
    if (n(c >= 0), d < 0 || b < 0)
      throw new e.ErrnoError(28);
    if (e.isClosed(l))
      throw new e.ErrnoError(8);
    if ((l.flags & 2097155) === 0)
      throw new e.ErrnoError(8);
    if (e.isDir(l.node.mode))
      throw new e.ErrnoError(31);
    if (!l.stream_ops.write)
      throw new e.ErrnoError(28);
    l.seekable && l.flags & 1024 && e.llseek(l, 0, 2);
    var t = typeof b < "u";
    if (!t)
      b = l.position;
    else if (!l.seekable)
      throw new e.ErrnoError(70);
    var i = l.stream_ops.write(l, Z, c, d, b, m);
    return t || (l.position += i), i;
  }, mmap(l, Z, c, d, b) {
    if ((d & 2) !== 0 && (b & 2) === 0 && (l.flags & 2097155) !== 2)
      throw new e.ErrnoError(2);
    if ((l.flags & 2097155) === 1)
      throw new e.ErrnoError(2);
    if (!l.stream_ops.mmap)
      throw new e.ErrnoError(43);
    if (!Z)
      throw new e.ErrnoError(28);
    return l.stream_ops.mmap(l, Z, c, d, b);
  }, msync(l, Z, c, d, b) {
    return n(c >= 0), l.stream_ops.msync ? l.stream_ops.msync(l, Z, c, d, b) : 0;
  }, ioctl(l, Z, c) {
    if (!l.stream_ops.ioctl)
      throw new e.ErrnoError(59);
    return l.stream_ops.ioctl(l, Z, c);
  }, readFile(l, Z = {}) {
    Z.flags = Z.flags || 0, Z.encoding = Z.encoding || "binary", Z.encoding !== "utf8" && Z.encoding !== "binary" && v(`Invalid encoding type "${Z.encoding}"`);
    var c = e.open(l, Z.flags), d = e.stat(l), b = d.size, m = new Uint8Array(b);
    return e.read(c, m, 0, b, 0), Z.encoding === "utf8" && (m = Xl(m)), e.close(c), m;
  }, writeFile(l, Z, c = {}) {
    c.flags = c.flags || 577;
    var d = e.open(l, c.flags, c.mode);
    typeof Z == "string" && (Z = new Uint8Array(tZ(Z))), ArrayBuffer.isView(Z) ? e.write(d, Z, 0, Z.byteLength, void 0, c.canOwn) : v("Unsupported data type"), e.close(d);
  }, cwd: () => e.currentPath, chdir(l) {
    var Z = e.lookupPath(l, { follow: !0 });
    if (Z.node === null)
      throw new e.ErrnoError(44);
    if (!e.isDir(Z.node.mode))
      throw new e.ErrnoError(54);
    var c = e.nodePermissions(Z.node, "x");
    if (c)
      throw new e.ErrnoError(c);
    e.currentPath = Z.path;
  }, createDefaultDirectories() {
    e.mkdir("/tmp"), e.mkdir("/home"), e.mkdir("/home/web_user");
  }, createDefaultDevices() {
    e.mkdir("/dev"), e.registerDevice(e.makedev(1, 3), { read: () => 0, write: (d, b, m, t, i) => t, llseek: () => 0 }), e.mkdev("/dev/null", e.makedev(1, 3)), Zl.register(e.makedev(5, 0), Zl.default_tty_ops), Zl.register(e.makedev(6, 0), Zl.default_tty1_ops), e.mkdev("/dev/tty", e.makedev(5, 0)), e.mkdev("/dev/tty1", e.makedev(6, 0));
    var l = new Uint8Array(1024), Z = 0, c = () => (Z === 0 && (bZ(l), Z = l.byteLength), l[--Z]);
    e.createDevice("/dev", "random", c), e.createDevice("/dev", "urandom", c), e.mkdir("/dev/shm"), e.mkdir("/dev/shm/tmp");
  }, createSpecialDirectories() {
    e.mkdir("/proc");
    var l = e.mkdir("/proc/self");
    e.mkdir("/proc/self/fd"), e.mount({ mount() {
      var Z = e.createNode(l, "fd", 16895, 73);
      return Z.stream_ops = { llseek: F.stream_ops.llseek }, Z.node_ops = { lookup(c, d) {
        var b = +d, m = e.getStreamChecked(b), t = { parent: null, mount: { mountpoint: "fake" }, node_ops: { readlink: () => m.path }, id: b + 1 };
        return t.parent = t, t;
      }, readdir() {
        return Array.from(e.streams.entries()).filter(([c, d]) => d).map(([c, d]) => c.toString());
      } }, Z;
    } }, {}, "/proc/self/fd");
  }, createStandardStreams(l, Z, c) {
    l ? e.createDevice("/dev", "stdin", l) : e.symlink("/dev/tty", "/dev/stdin"), Z ? e.createDevice("/dev", "stdout", null, Z) : e.symlink("/dev/tty", "/dev/stdout"), c ? e.createDevice("/dev", "stderr", null, c) : e.symlink("/dev/tty1", "/dev/stderr");
    var d = e.open("/dev/stdin", 0), b = e.open("/dev/stdout", 1), m = e.open("/dev/stderr", 1);
    n(d.fd === 0, `invalid handle for stdin (${d.fd})`), n(b.fd === 1, `invalid handle for stdout (${b.fd})`), n(m.fd === 2, `invalid handle for stderr (${m.fd})`);
  }, staticInit() {
    e.nameTable = new Array(4096), e.mount(F, {}, "/"), e.createDefaultDirectories(), e.createDefaultDevices(), e.createSpecialDirectories(), e.filesystems = { MEMFS: F };
  }, init(l, Z, c) {
    n(!e.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), e.initialized = !0, l ??= V.stdin, Z ??= V.stdout, c ??= V.stderr, e.createStandardStreams(l, Z, c);
  }, quit() {
    e.initialized = !1, GZ(0);
    for (var l of e.streams)
      l && e.close(l);
  }, findObject(l, Z) {
    var c = e.analyzePath(l, Z);
    return c.exists ? c.object : null;
  }, analyzePath(l, Z) {
    try {
      var c = e.lookupPath(l, { follow: !Z });
      l = c.path;
    } catch {
    }
    var d = { isRoot: !1, exists: !1, error: 0, name: null, path: null, object: null, parentExists: !1, parentPath: null, parentObject: null };
    try {
      var c = e.lookupPath(l, { parent: !0 });
      d.parentExists = !0, d.parentPath = c.path, d.parentObject = c.node, d.name = U.basename(l), c = e.lookupPath(l, { follow: !Z }), d.exists = !0, d.path = c.path, d.object = c.node, d.name = c.node.name, d.isRoot = c.path === "/";
    } catch (b) {
      d.error = b.errno;
    }
    return d;
  }, createPath(l, Z, c, d) {
    l = typeof l == "string" ? l : e.getPath(l);
    for (var b = Z.split("/").reverse(); b.length; ) {
      var m = b.pop();
      if (m) {
        var t = U.join2(l, m);
        try {
          e.mkdir(t);
        } catch (i) {
          if (i.errno != 20) throw i;
        }
        l = t;
      }
    }
    return t;
  }, createFile(l, Z, c, d, b) {
    var m = U.join2(typeof l == "string" ? l : e.getPath(l), Z), t = iZ(d, b);
    return e.create(m, t);
  }, createDataFile(l, Z, c, d, b, m) {
    var t = Z;
    l && (l = typeof l == "string" ? l : e.getPath(l), t = Z ? U.join2(l, Z) : l);
    var i = iZ(d, b), W = e.create(t, i);
    if (c) {
      if (typeof c == "string") {
        for (var X = new Array(c.length), h = 0, s = c.length; h < s; ++h) X[h] = c.charCodeAt(h);
        c = X;
      }
      e.chmod(W, i | 146);
      var G = e.open(W, 577);
      e.write(G, c, 0, c.length, 0, m), e.close(G), e.chmod(W, i);
    }
  }, createDevice(l, Z, c, d) {
    var b = U.join2(typeof l == "string" ? l : e.getPath(l), Z), m = iZ(!!c, !!d);
    e.createDevice.major ??= 64;
    var t = e.makedev(e.createDevice.major++, 0);
    return e.registerDevice(t, { open(i) {
      i.seekable = !1;
    }, close(i) {
      d?.buffer?.length && d(10);
    }, read(i, W, X, h, s) {
      for (var G = 0, o = 0; o < h; o++) {
        var J;
        try {
          J = c();
        } catch {
          throw new e.ErrnoError(29);
        }
        if (J === void 0 && G === 0)
          throw new e.ErrnoError(6);
        if (J == null) break;
        G++, W[X + o] = J;
      }
      return G && (i.node.atime = Date.now()), G;
    }, write(i, W, X, h, s) {
      for (var G = 0; G < h; G++)
        try {
          d(W[X + G]);
        } catch {
          throw new e.ErrnoError(29);
        }
      return h && (i.node.mtime = i.node.ctime = Date.now()), G;
    } }), e.mkdev(b, m, t);
  }, forceLoadFile(l) {
    if (l.isDevice || l.isFolder || l.link || l.contents) return !0;
    if (globalThis.XMLHttpRequest)
      v("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    else
      try {
        l.contents = Ul(l.url);
      } catch {
        throw new e.ErrnoError(29);
      }
  }, createLazyFile(l, Z, c, d, b) {
    class m {
      lengthKnown = !1;
      chunks = [];
      get(G) {
        if (!(G > this.length - 1 || G < 0)) {
          var o = G % this.chunkSize, J = G / this.chunkSize | 0;
          return this.getter(J)[o];
        }
      }
      setDataGetter(G) {
        this.getter = G;
      }
      cacheLength() {
        var G = new XMLHttpRequest();
        G.open("HEAD", c, !1), G.send(null), G.status >= 200 && G.status < 300 || G.status === 304 || v("Couldn't load " + c + ". Status: " + G.status);
        var o = Number(G.getResponseHeader("Content-length")), J, T = (J = G.getResponseHeader("Accept-Ranges")) && J === "bytes", k = (J = G.getResponseHeader("Content-Encoding")) && J === "gzip", g = 1024 * 1024;
        T || (g = o);
        var K = (j, sl) => {
          j > sl && v("invalid range (" + j + ", " + sl + ") or no bytes requested!"), sl > o - 1 && v("only " + o + " bytes available! programmer error!");
          var x = new XMLHttpRequest();
          return x.open("GET", c, !1), o !== g && x.setRequestHeader("Range", "bytes=" + j + "-" + sl), x.responseType = "arraybuffer", x.overrideMimeType && x.overrideMimeType("text/plain; charset=x-user-defined"), x.send(null), x.status >= 200 && x.status < 300 || x.status === 304 || v("Couldn't load " + c + ". Status: " + x.status), x.response !== void 0 ? new Uint8Array(x.response || []) : tZ(x.responseText || "");
        }, kl = this;
        kl.setDataGetter((j) => {
          var sl = j * g, x = (j + 1) * g - 1;
          return x = Math.min(x, o - 1), typeof kl.chunks[j] > "u" && (kl.chunks[j] = K(sl, x)), typeof kl.chunks[j] > "u" && v("doXHR failed!"), kl.chunks[j];
        }), (k || !o) && (g = o = 1, o = this.getter(0).length, g = o, q("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = o, this._chunkSize = g, this.lengthKnown = !0;
      }
      get length() {
        return this.lengthKnown || this.cacheLength(), this._length;
      }
      get chunkSize() {
        return this.lengthKnown || this.cacheLength(), this._chunkSize;
      }
    }
    if (globalThis.XMLHttpRequest) {
      $ || v("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
      var t = new m(), i = { isDevice: !1, contents: t };
    } else
      var i = { isDevice: !1, url: c };
    var W = e.createFile(l, Z, i, d, b);
    i.contents ? W.contents = i.contents : i.url && (W.contents = null, W.url = i.url), Object.defineProperties(W, { usedBytes: { get: function() {
      return this.contents.length;
    } } });
    var X = {};
    for (const [s, G] of Object.entries(W.stream_ops))
      X[s] = (...o) => (e.forceLoadFile(W), G(...o));
    function h(s, G, o, J, T) {
      var k = s.node.contents;
      if (T >= k.length) return 0;
      var g = Math.min(k.length - T, J);
      if (n(g >= 0), k.slice)
        for (var K = 0; K < g; K++)
          G[o + K] = k[T + K];
      else
        for (var K = 0; K < g; K++)
          G[o + K] = k.get(T + K);
      return g;
    }
    return X.read = (s, G, o, J, T) => (e.forceLoadFile(W), h(s, G, o, J, T)), X.mmap = (s, G, o, J, T) => {
      e.forceLoadFile(W);
      var k = qZ(G);
      if (!k)
        throw new e.ErrnoError(48);
      return h(s, (a(), S), k, G, o), { ptr: k, allocated: !0 };
    }, W.stream_ops = X, W;
  }, absolutePath() {
    v("FS.absolutePath has been removed; use PATH_FS.resolve instead");
  }, createFolder() {
    v("FS.createFolder has been removed; use FS.mkdir instead");
  }, createLink() {
    v("FS.createLink has been removed; use FS.symlink instead");
  }, joinPath() {
    v("FS.joinPath has been removed; use PATH.join instead");
  }, mmapAlloc() {
    v("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
  }, standardizePath() {
    v("FS.standardizePath has been removed; use PATH.normalize instead");
  } }, H = { DEFAULT_POLLMASK: 5, calculateAt(l, Z, c) {
    if (U.isAbs(Z))
      return Z;
    var d;
    if (l === -100)
      d = e.cwd();
    else {
      var b = H.getStreamFromFD(l);
      d = b.path;
    }
    if (Z.length == 0) {
      if (!c)
        throw new e.ErrnoError(44);
      return d;
    }
    return d + "/" + Z;
  }, writeStat(l, Z) {
    (a(), R)[l >> 2] = Z.dev, (a(), R)[l + 4 >> 2] = Z.mode, (a(), R)[l + 8 >> 2] = Z.nlink, (a(), R)[l + 12 >> 2] = Z.uid, (a(), R)[l + 16 >> 2] = Z.gid, (a(), R)[l + 20 >> 2] = Z.rdev, (a(), B)[l + 24 >> 3] = BigInt(Z.size), (a(), f)[l + 32 >> 2] = 4096, (a(), f)[l + 36 >> 2] = Z.blocks;
    var c = Z.atime.getTime(), d = Z.mtime.getTime(), b = Z.ctime.getTime();
    return (a(), B)[l + 40 >> 3] = BigInt(Math.floor(c / 1e3)), (a(), R)[l + 48 >> 2] = c % 1e3 * 1e3 * 1e3, (a(), B)[l + 56 >> 3] = BigInt(Math.floor(d / 1e3)), (a(), R)[l + 64 >> 2] = d % 1e3 * 1e3 * 1e3, (a(), B)[l + 72 >> 3] = BigInt(Math.floor(b / 1e3)), (a(), R)[l + 80 >> 2] = b % 1e3 * 1e3 * 1e3, (a(), B)[l + 88 >> 3] = BigInt(Z.ino), 0;
  }, writeStatFs(l, Z) {
    (a(), R)[l + 4 >> 2] = Z.bsize, (a(), R)[l + 60 >> 2] = Z.bsize, (a(), B)[l + 8 >> 3] = BigInt(Z.blocks), (a(), B)[l + 16 >> 3] = BigInt(Z.bfree), (a(), B)[l + 24 >> 3] = BigInt(Z.bavail), (a(), B)[l + 32 >> 3] = BigInt(Z.files), (a(), B)[l + 40 >> 3] = BigInt(Z.ffree), (a(), R)[l + 48 >> 2] = Z.fsid, (a(), R)[l + 64 >> 2] = Z.flags, (a(), R)[l + 56 >> 2] = Z.namelen;
  }, doMsync(l, Z, c, d, b) {
    if (!e.isFile(Z.node.mode))
      throw new e.ErrnoError(43);
    if (d & 2)
      return 0;
    var m = (a(), w).slice(l, l + c);
    e.msync(Z, m, b, c, d);
  }, getStreamFromFD(l) {
    var Z = e.getStreamChecked(l);
    return Z;
  }, varargs: void 0, getStr(l) {
    var Z = _(l);
    return Z;
  } };
  function dc(l, Z, c) {
    if (p) return z(3, 0, 1, l, Z, c);
    H.varargs = c;
    try {
      var d = H.getStreamFromFD(l);
      switch (Z) {
        case 0: {
          var b = gl();
          if (b < 0)
            return -28;
          for (; e.streams[b]; )
            b++;
          var m;
          return m = e.dupStream(d, b), m.fd;
        }
        case 1:
        case 2:
          return 0;
        case 3:
          return d.flags;
        case 4: {
          var b = gl();
          return d.flags |= b, 0;
        }
        case 12: {
          var b = rl(), t = 0;
          return (a(), dl)[b + t >> 1] = 2, 0;
        }
        case 13:
        case 14:
          return 0;
      }
      return -28;
    } catch (i) {
      if (typeof e > "u" || i.name !== "ErrnoError") throw i;
      return -i.errno;
    }
  }
  function ec(l, Z) {
    if (p) return z(4, 0, 1, l, Z);
    try {
      return H.writeStat(Z, e.fstat(l));
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return -c.errno;
    }
  }
  function bc(l, Z, c) {
    if (p) return z(5, 0, 1, l, Z, c);
    H.varargs = c;
    try {
      var d = H.getStreamFromFD(l);
      switch (Z) {
        case 21509:
          return d.tty ? 0 : -59;
        case 21505: {
          if (!d.tty) return -59;
          if (d.tty.ops.ioctl_tcgets) {
            var b = d.tty.ops.ioctl_tcgets(d), m = rl();
            (a(), f)[m >> 2] = b.c_iflag || 0, (a(), f)[m + 4 >> 2] = b.c_oflag || 0, (a(), f)[m + 8 >> 2] = b.c_cflag || 0, (a(), f)[m + 12 >> 2] = b.c_lflag || 0;
            for (var t = 0; t < 32; t++)
              (a(), S)[m + t + 17] = b.c_cc[t] || 0;
            return 0;
          }
          return 0;
        }
        case 21510:
        case 21511:
        case 21512:
          return d.tty ? 0 : -59;
        case 21506:
        case 21507:
        case 21508: {
          if (!d.tty) return -59;
          if (d.tty.ops.ioctl_tcsets) {
            for (var m = rl(), i = (a(), f)[m >> 2], W = (a(), f)[m + 4 >> 2], X = (a(), f)[m + 8 >> 2], h = (a(), f)[m + 12 >> 2], s = [], t = 0; t < 32; t++)
              s.push((a(), S)[m + t + 17]);
            return d.tty.ops.ioctl_tcsets(d.tty, Z, { c_iflag: i, c_oflag: W, c_cflag: X, c_lflag: h, c_cc: s });
          }
          return 0;
        }
        case 21519: {
          if (!d.tty) return -59;
          var m = rl();
          return (a(), f)[m >> 2] = 0, 0;
        }
        case 21520:
          return d.tty ? -28 : -59;
        case 21537:
        case 21531: {
          var m = rl();
          return e.ioctl(d, Z, m);
        }
        case 21523: {
          if (!d.tty) return -59;
          if (d.tty.ops.ioctl_tiocgwinsz) {
            var G = d.tty.ops.ioctl_tiocgwinsz(d.tty), m = rl();
            (a(), dl)[m >> 1] = G[0], (a(), dl)[m + 2 >> 1] = G[1];
          }
          return 0;
        }
        case 21524:
          return d.tty ? 0 : -59;
        case 21515:
          return d.tty ? 0 : -59;
        default:
          return -28;
      }
    } catch (o) {
      if (typeof e > "u" || o.name !== "ErrnoError") throw o;
      return -o.errno;
    }
  }
  function mc(l, Z) {
    if (p) return z(6, 0, 1, l, Z);
    try {
      return l = H.getStr(l), H.writeStat(Z, e.lstat(l));
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return -c.errno;
    }
  }
  function tc(l, Z, c, d) {
    if (p) return z(7, 0, 1, l, Z, c, d);
    try {
      Z = H.getStr(Z);
      var b = d & 256, m = d & 4096;
      return d = d & -6401, n(!d, `unknown flags in __syscall_newfstatat: ${d}`), Z = H.calculateAt(l, Z, m), H.writeStat(c, b ? e.lstat(Z) : e.stat(Z));
    } catch (t) {
      if (typeof e > "u" || t.name !== "ErrnoError") throw t;
      return -t.errno;
    }
  }
  function ic(l, Z, c, d) {
    if (p) return z(8, 0, 1, l, Z, c, d);
    H.varargs = d;
    try {
      Z = H.getStr(Z), Z = H.calculateAt(l, Z);
      var b = d ? gl() : 0;
      return e.open(Z, c, b).fd;
    } catch (m) {
      if (typeof e > "u" || m.name !== "ErrnoError") throw m;
      return -m.errno;
    }
  }
  function nc(l, Z) {
    if (p) return z(9, 0, 1, l, Z);
    try {
      return l = H.getStr(l), H.writeStat(Z, e.stat(l));
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return -c.errno;
    }
  }
  var Td = () => v("native code called abort()"), C = (l) => {
    for (var Z = ""; ; ) {
      var c = (a(), w)[l++];
      if (!c) return Z;
      Z += String.fromCharCode(c);
    }
  }, Gl = {}, hl = {}, Bl = {}, fd = class extends Error {
    constructor(Z) {
      super(Z), this.name = "BindingError";
    }
  }, Q = (l) => {
    throw new fd(l);
  };
  function gd(l, Z, c = {}) {
    var d = Z.name;
    if (l || Q(`type "${d}" must have a positive integer typeid pointer`), hl.hasOwnProperty(l)) {
      if (c.ignoreDuplicateRegistrations)
        return;
      Q(`Cannot register type '${d}' twice`);
    }
    if (hl[l] = Z, delete Bl[l], Gl.hasOwnProperty(l)) {
      var b = Gl[l];
      delete Gl[l], b.forEach((m) => m());
    }
  }
  function E(l, Z, c = {}) {
    return gd(l, Z, c);
  }
  var Wc = (l, Z, c) => {
    switch (Z) {
      case 1:
        return c ? (d) => (a(), S)[d] : (d) => (a(), w)[d];
      case 2:
        return c ? (d) => (a(), dl)[d >> 1] : (d) => (a(), Fl)[d >> 1];
      case 4:
        return c ? (d) => (a(), f)[d >> 2] : (d) => (a(), R)[d >> 2];
      case 8:
        return c ? (d) => (a(), B)[d >> 3] : (d) => (a(), UZ)[d >> 3];
      default:
        throw new TypeError(`invalid integer width (${Z}): ${l}`);
    }
  }, zl = (l) => {
    if (l === null)
      return "null";
    var Z = typeof l;
    return Z === "object" || Z === "array" || Z === "function" ? l.toString() : "" + l;
  }, ac = (l, Z, c, d) => {
    if (Z < c || Z > d)
      throw new TypeError(`Passing a number "${zl(Z)}" from JS side to C/C++ side to an argument of type "${l}", which is outside the valid range [${c}, ${d}]!`);
  }, Bd = (l, Z, c, d, b) => {
    Z = C(Z);
    const m = d === 0n;
    let t = (i) => i;
    if (m) {
      const i = c * 8;
      t = (W) => BigInt.asUintN(i, W), b = t(b);
    }
    E(l, { name: Z, fromWireType: t, toWireType: (i, W) => {
      if (typeof W == "number")
        W = BigInt(W);
      else if (typeof W != "bigint")
        throw new TypeError(`Cannot convert "${zl(W)}" to ${this.name}`);
      return ac(Z, W, d, b), W;
    }, readValueFromPointer: Wc(Z, c, !m), destructorFunction: null });
  }, zd = (l, Z, c, d) => {
    Z = C(Z), E(l, { name: Z, fromWireType: function(b) {
      return !!b;
    }, toWireType: function(b, m) {
      return m ? c : d;
    }, readValueFromPointer: function(b) {
      return this.fromWireType((a(), w)[b]);
    }, destructorFunction: null });
  }, Vc = [], O = [0, 1, , 1, null, 1, !0, 1, !1, 1], Sd = (l) => {
    l > 9 && --O[l + 1] === 0 && (n(O[l] !== void 0, "Decref for unallocated handle."), O[l] = void 0, Vc.push(l));
  }, Xc = { toValue: (l) => (l || Q(`Cannot use deleted val. handle = ${l}`), n(l === 2 || O[l] !== void 0 && l % 2 === 0, `invalid handle: ${l}`), O[l]), toHandle: (l) => {
    switch (l) {
      case void 0:
        return 2;
      case null:
        return 4;
      case !0:
        return 6;
      case !1:
        return 8;
      default: {
        const Z = Vc.pop() || O.length;
        return O[Z] = l, O[Z + 1] = 1, Z;
      }
    }
  } };
  function nZ(l) {
    return this.fromWireType((a(), R)[l >> 2]);
  }
  var xd = { name: "emscripten::val", fromWireType: (l) => {
    var Z = Xc.toValue(l);
    return Sd(l), Z;
  }, toWireType: (l, Z) => Xc.toHandle(Z), readValueFromPointer: nZ, destructorFunction: null }, Kd = (l) => E(l, xd), wd = (l, Z) => {
    switch (Z) {
      case 4:
        return function(c) {
          return this.fromWireType((a(), vZ)[c >> 2]);
        };
      case 8:
        return function(c) {
          return this.fromWireType((a(), Yl)[c >> 3]);
        };
      default:
        throw new TypeError(`invalid float width (${Z}): ${l}`);
    }
  }, Cd = (l, Z, c) => {
    Z = C(Z), E(l, { name: Z, fromWireType: (d) => d, toWireType: (d, b) => {
      if (typeof b != "number" && typeof b != "boolean")
        throw new TypeError(`Cannot convert ${zl(b)} to ${this.name}`);
      return b;
    }, readValueFromPointer: wd(Z, c), destructorFunction: null });
  }, rc = (l, Z) => Object.defineProperty(Z, "name", { value: l }), Qd = (l) => {
    for (; l.length; ) {
      var Z = l.pop(), c = l.pop();
      c(Z);
    }
  };
  function yc(l) {
    for (var Z = 1; Z < l.length; ++Z)
      if (l[Z] !== null && l[Z].destructorFunction === void 0)
        return !0;
    return !1;
  }
  function Ld(l, Z, c, d, b) {
    if (l < Z || l > c) {
      var m = Z == c ? Z : `${Z} to ${c}`;
      b(`function ${d} called with ${l} arguments, expected ${m}`);
    }
  }
  function Ed(l, Z, c, d) {
    for (var b = yc(l), m = l.length - 2, t = [], i = ["fn"], W = 0; W < m; ++W)
      t.push(`arg${W}`), i.push(`arg${W}Wired`);
    t = t.join(","), i = i.join(",");
    var X = `return function (${t}) {
`;
    X += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, b && (X += `var destructors = [];
`);
    for (var h = b ? "destructors" : "null", s = ["humanName", "throwBindingError", "invoker", "fn", "runDestructors", "fromRetWire", "toClassParamWire"], W = 0; W < m; ++W) {
      var G = `toArg${W}Wire`;
      X += `var arg${W}Wired = ${G}(${h}, arg${W});
`, s.push(G);
    }
    X += (c || d ? "var rv = " : "") + `invoker(${i});
`;
    var o = c ? "rv" : "";
    if (s.push("Asyncify"), X += `function onDone(${o}) {
`, b)
      X += `runDestructors(destructors);
`;
    else
      for (var W = 2; W < l.length; ++W) {
        var J = W === 1 ? "thisWired" : "arg" + (W - 2) + "Wired";
        l[W].destructorFunction !== null && (X += `${J}_dtor(${J});
`, s.push(`${J}_dtor`));
      }
    return c && (X += `var ret = fromRetWire(rv);
return ret;
`), X += `}
`, X += `return Asyncify.currData ? Asyncify.whenDone().then(onDone) : onDone(${o});
`, X += `}
`, s.push("checkArgCount", "minArgs", "maxArgs"), X = `if (arguments.length !== ${s.length}){ throw new Error(humanName + "Expected ${s.length} closure arguments " + arguments.length + " given."); }
${X}`, new Function(s, X);
  }
  var Sl = (l) => {
    try {
      return l();
    } catch (Z) {
      v(Z);
    }
  }, Gc = (l) => {
    if (l instanceof gZ || l == "unwind")
      return nl;
    pl(), l instanceof WebAssembly.RuntimeError && sZ() <= 0 && Y("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), FZ(1, l);
  }, Md = () => {
    if (!fl())
      try {
        if (p) {
          Rl() && oZ(nl);
          return;
        }
        dZ(nl);
      } catch (l) {
        Gc(l);
      }
  }, WZ = (l) => {
    if (P) {
      Y("user callback triggered after runtime exited or application aborted.  Ignoring.");
      return;
    }
    try {
      l(), Md();
    } catch (Z) {
      Gc(Z);
    }
  }, aZ = () => {
    Vl += 1;
  }, hc = () => {
    n(Vl > 0), Vl -= 1;
  }, r = { instrumentWasmImports(l) {
    var Z = /^(invoke_.*|__asyncjs__.*)$/;
    for (let [c, d] of Object.entries(l))
      if (typeof d == "function") {
        let b = d.isAsync || Z.test(c);
        l[c] = (...m) => {
          var t = r.state;
          try {
            return d(...m);
          } finally {
            var i = t === r.State.Normal && r.state === r.State.Disabled, W = c.startsWith("invoke_") && !0;
            r.state !== t && !b && !i && !W && v(`import ${c} was not in ASYNCIFY_IMPORTS, but changed the state`);
          }
        };
      }
  }, instrumentFunction(l) {
    var Z = (...c) => {
      r.exportCallStack.push(l);
      try {
        return l(...c);
      } finally {
        if (!P) {
          var d = r.exportCallStack.pop();
          n(d === l), r.maybeStopUnwind();
        }
      }
    };
    return r.funcWrappers.set(l, Z), Z = rc(`__asyncify_wrapper_${l.name}`, Z), Z;
  }, instrumentWasmExports(l) {
    var Z = {};
    for (let [d, b] of Object.entries(l))
      if (typeof b == "function") {
        var c = r.instrumentFunction(b);
        Z[d] = c;
      } else
        Z[d] = b;
    return Z;
  }, State: { Normal: 0, Unwinding: 1, Rewinding: 2, Disabled: 3 }, state: 0, StackSize: 4096, currData: null, handleSleepReturnValue: 0, exportCallStack: [], callstackFuncToId: /* @__PURE__ */ new Map(), callStackIdToFunc: /* @__PURE__ */ new Map(), funcWrappers: /* @__PURE__ */ new Map(), callStackId: 0, asyncPromiseHandlers: null, sleepCallbacks: [], getCallStackId(l) {
    if (n(l), !r.callstackFuncToId.has(l)) {
      var Z = r.callStackId++;
      r.callstackFuncToId.set(l, Z), r.callStackIdToFunc.set(Z, l);
    }
    return r.callstackFuncToId.get(l);
  }, maybeStopUnwind() {
    r.currData && r.state === r.State.Unwinding && r.exportCallStack.length === 0 && (r.state = r.State.Normal, aZ(), Sl(_c), typeof Fibers < "u" && Fibers.trampoline());
  }, whenDone() {
    return n(r.currData, "Tried to wait for an async operation when none is in progress."), n(!r.asyncPromiseHandlers, "Cannot have multiple async operations in flight at once"), new Promise((l, Z) => {
      r.asyncPromiseHandlers = { resolve: l, reject: Z };
    });
  }, allocateData() {
    var l = Nl(12 + r.StackSize);
    return r.setDataHeader(l, l + 12, r.StackSize), r.setDataRewindFunc(l), l;
  }, setDataHeader(l, Z, c) {
    (a(), R)[l >> 2] = Z, (a(), R)[l + 4 >> 2] = Z + c;
  }, setDataRewindFunc(l) {
    var Z = r.exportCallStack[0];
    n(Z, "exportCallStack is empty");
    var c = r.getCallStackId(Z);
    (a(), f)[l + 8 >> 2] = c;
  }, getDataRewindFunc(l) {
    var Z = (a(), f)[l + 8 >> 2], c = r.callStackIdToFunc.get(Z);
    return n(c, `id ${Z} not found in callStackIdToFunc`), c;
  }, doRewind(l) {
    var Z = r.getDataRewindFunc(l), c = r.funcWrappers.get(Z);
    return n(Z), n(c), hc(), c();
  }, handleSleep(l) {
    if (n(r.state !== r.State.Disabled, "Asyncify cannot be done during or after the runtime exits"), !P) {
      if (r.state === r.State.Normal) {
        var Z = !1, c = !1;
        l((d = 0) => {
          if (n(!d || typeof d == "number" || typeof d == "boolean"), !P && (r.handleSleepReturnValue = d, Z = !0, !!c)) {
            n(!r.exportCallStack.length, "Waking up (starting to rewind) must be done from JS, without compiled code on the stack."), r.state = r.State.Rewinding, Sl(() => Oc(r.currData)), typeof MainLoop < "u" && MainLoop.func && MainLoop.resume();
            var b, m = !1;
            try {
              b = r.doRewind(r.currData);
            } catch (W) {
              b = W, m = !0;
            }
            var t = !1;
            if (!r.currData) {
              var i = r.asyncPromiseHandlers;
              i && (r.asyncPromiseHandlers = null, (m ? i.reject : i.resolve)(b), t = !0);
            }
            if (m && !t)
              throw b;
          }
        }), c = !0, Z || (r.state = r.State.Unwinding, r.currData = r.allocateData(), typeof MainLoop < "u" && MainLoop.func && MainLoop.pause(), Sl(() => Pc(r.currData)));
      } else r.state === r.State.Rewinding ? (r.state = r.State.Normal, Sl(Dc), M(r.currData), r.currData = null, r.sleepCallbacks.forEach(WZ)) : v(`invalid state: ${r.state}`);
      return r.handleSleepReturnValue;
    }
  }, handleAsync: (l) => r.handleSleep((Z) => {
    l().then(Z);
  }) };
  function jd(l) {
    for (var Z = l.length - 2, c = l.length - 1; c >= 2 && l[c].optional; --c)
      Z--;
    return Z;
  }
  function Pd(l, Z, c, d, b, m) {
    var t = Z.length;
    t < 2 && Q("argTypes array size mismatch! Must at least get return value and 'this' types!"), n(!m, "Async bindings are only supported with JSPI.");
    for (var i = Z[1] !== null && c !== null, W = yc(Z), X = !Z[0].isVoid, h = t - 2, s = jd(Z), G = Z[0], o = Z[1], J = [l, Q, d, b, Qd, G.fromWireType.bind(G), o?.toWireType.bind(o)], T = 2; T < t; ++T) {
      var k = Z[T];
      J.push(k.toWireType.bind(k));
    }
    if (J.push(r), !W)
      for (var T = 2; T < Z.length; ++T)
        Z[T].destructorFunction !== null && J.push(Z[T].destructorFunction);
    J.push(Ld, s, h);
    var K = Ed(Z, i, X, m)(...J);
    return rc(l, K);
  }
  var _d = (l, Z, c) => {
    if (l[Z].overloadTable === void 0) {
      var d = l[Z];
      l[Z] = function(...b) {
        return l[Z].overloadTable.hasOwnProperty(b.length) || Q(`Function '${c}' called with an invalid number of arguments (${b.length}) - expects one of (${l[Z].overloadTable})!`), l[Z].overloadTable[b.length].apply(this, b);
      }, l[Z].overloadTable = [], l[Z].overloadTable[d.argCount] = d;
    }
  }, Od = (l, Z, c) => {
    V.hasOwnProperty(l) ? ((c === void 0 || V[l].overloadTable !== void 0 && V[l].overloadTable[c] !== void 0) && Q(`Cannot register public name '${l}' twice`), _d(V, l, l), V[l].overloadTable.hasOwnProperty(c) && Q(`Cannot register multiple overloads of a function with the same number of arguments (${c})!`), V[l].overloadTable[c] = Z) : (V[l] = Z, V[l].argCount = c);
  }, Dd = (l, Z) => {
    for (var c = [], d = 0; d < l; d++)
      c.push((a(), R)[Z + d * 4 >> 2]);
    return c;
  }, Ad = class extends Error {
    constructor(Z) {
      super(Z), this.name = "InternalError";
    }
  }, Rc = (l) => {
    throw new Ad(l);
  }, $d = (l, Z, c) => {
    V.hasOwnProperty(l) || Rc("Replacing nonexistent public symbol"), V[l].overloadTable !== void 0 && c !== void 0 ? V[l].overloadTable[c] = Z : (V[l] = Z, V[l].argCount = c);
  }, qd = (l, Z, c = !1) => (...d) => yd(l, Z, d, c), le = (l, Z, c = !1) => {
    n(!c, "Async bindings are only supported with JSPI."), l = C(l);
    function d() {
      return qd(l, Z);
    }
    var b = d();
    return typeof b != "function" && Q(`unknown function pointer with signature ${l}: ${Z}`), b;
  };
  class Ze extends Error {
  }
  var ce = (l) => {
    var Z = gc(l), c = C(Z);
    return M(Z), c;
  }, de = (l, Z) => {
    var c = [], d = {};
    function b(m) {
      if (!d[m] && !hl[m]) {
        if (Bl[m]) {
          Bl[m].forEach(b);
          return;
        }
        c.push(m), d[m] = !0;
      }
    }
    throw Z.forEach(b), new Ze(`${l}: ` + c.map(ce).join([", "]));
  }, ee = (l, Z, c) => {
    l.forEach((i) => Bl[i] = Z);
    function d(i) {
      var W = c(i);
      W.length !== l.length && Rc("Mismatched type converter count");
      for (var X = 0; X < l.length; ++X)
        E(l[X], W[X]);
    }
    var b = new Array(Z.length), m = [], t = 0;
    for (let [i, W] of Z.entries())
      hl.hasOwnProperty(W) ? b[i] = hl[W] : (m.push(W), Gl.hasOwnProperty(W) || (Gl[W] = []), Gl[W].push(() => {
        b[i] = hl[W], ++t, t === m.length && d(b);
      }));
    m.length === 0 && d(b);
  }, be = (l) => {
    l = l.trim();
    const Z = l.indexOf("(");
    return Z === -1 ? l : (n(l.endsWith(")"), "Parentheses for argument names should match."), l.slice(0, Z));
  }, me = (l, Z, c, d, b, m, t, i) => {
    var W = Dd(Z, c);
    l = C(l), l = be(l), b = le(d, b, t), Od(l, function() {
      de(`Cannot call ${l} due to unbound types`, W);
    }, Z - 1), ee([], W, (X) => {
      var h = [X[0], null].concat(X.slice(1));
      return $d(l, Pd(l, h, null, b, m, t), Z - 1), [];
    });
  }, te = (l, Z, c, d, b) => {
    Z = C(Z);
    const m = d === 0;
    let t = (W) => W;
    if (m) {
      var i = 32 - 8 * c;
      t = (W) => W << i >>> i, b = t(b);
    }
    E(l, { name: Z, fromWireType: t, toWireType: (W, X) => {
      if (typeof X != "number" && typeof X != "boolean")
        throw new TypeError(`Cannot convert "${zl(X)}" to ${Z}`);
      return ac(Z, X, d, b), X;
    }, readValueFromPointer: Wc(Z, c, d !== 0), destructorFunction: null });
  }, ie = (l, Z, c) => {
    var d = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array, BigInt64Array, BigUint64Array], b = d[Z];
    function m(t) {
      var i = (a(), R)[t >> 2], W = (a(), R)[t + 4 >> 2];
      return new b((a(), S).buffer, W, i);
    }
    c = C(c), E(l, { name: c, fromWireType: m, readValueFromPointer: m }, { ignoreDuplicateRegistrations: !0 });
  }, D = (l, Z, c) => (n(typeof c == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), AZ(l, (a(), w), Z, c)), ne = (l, Z) => {
    Z = C(Z), E(l, { name: Z, fromWireType(c) {
      var d = (a(), R)[c >> 2], b = c + 4, m;
      return m = _(b, d, !0), M(c), m;
    }, toWireType(c, d) {
      d instanceof ArrayBuffer && (d = new Uint8Array(d));
      var b, m = typeof d == "string";
      m || ArrayBuffer.isView(d) && d.BYTES_PER_ELEMENT == 1 || Q("Cannot pass non-string to std::string"), m ? b = tl(d) : b = d.length;
      var t = Nl(4 + b + 1), i = t + 4;
      return (a(), R)[t >> 2] = b, m ? D(d, i, b + 1) : (a(), w).set(d, i), c !== null && c.push(M, t), t;
    }, readValueFromPointer: nZ, destructorFunction(c) {
      M(c);
    } });
  }, oc = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, We = (l, Z, c) => {
    n(l % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
    var d = l >> 1, b = _Z((a(), Fl), d, Z / 2, c);
    if (b - d > 16 && oc) return oc.decode((a(), Fl).slice(d, b));
    for (var m = "", t = d; t < b; ++t) {
      var i = (a(), Fl)[t];
      m += String.fromCharCode(i);
    }
    return m;
  }, ae = (l, Z, c) => {
    if (n(Z % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), n(typeof c == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), c ??= 2147483647, c < 2) return 0;
    c -= 2;
    for (var d = Z, b = c < l.length * 2 ? c / 2 : l.length, m = 0; m < b; ++m) {
      var t = l.charCodeAt(m);
      (a(), dl)[Z >> 1] = t, Z += 2;
    }
    return (a(), dl)[Z >> 1] = 0, Z - d;
  }, Ve = (l) => l.length * 2, Xe = (l, Z, c) => {
    n(l % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
    for (var d = "", b = l >> 2, m = 0; !(m >= Z / 4); m++) {
      var t = (a(), R)[b + m];
      if (!t && !c) break;
      d += String.fromCodePoint(t);
    }
    return d;
  }, re = (l, Z, c) => {
    if (n(Z % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), n(typeof c == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), c ??= 2147483647, c < 4) return 0;
    for (var d = Z, b = d + c - 4, m = 0; m < l.length; ++m) {
      var t = l.codePointAt(m);
      if (t > 65535 && m++, (a(), f)[Z >> 2] = t, Z += 4, Z + 4 > b) break;
    }
    return (a(), f)[Z >> 2] = 0, Z - d;
  }, ye = (l) => {
    for (var Z = 0, c = 0; c < l.length; ++c) {
      var d = l.codePointAt(c);
      d > 65535 && c++, Z += 4;
    }
    return Z;
  }, Ge = (l, Z, c) => {
    c = C(c);
    var d, b, m;
    Z === 2 ? (d = We, b = ae, m = Ve) : (n(Z === 4, "only 2-byte and 4-byte strings are currently supported"), d = Xe, b = re, m = ye), E(l, { name: c, fromWireType: (t) => {
      var i = (a(), R)[t >> 2], W = d(t + 4, i * Z, !0);
      return M(t), W;
    }, toWireType: (t, i) => {
      typeof i != "string" && Q(`Cannot pass non-string to C++ string type ${c}`);
      var W = m(i), X = Nl(4 + W + Z);
      return (a(), R)[X >> 2] = W / Z, b(i, X + 4, W + Z), t !== null && t.push(M, X), X;
    }, readValueFromPointer: nZ, destructorFunction(t) {
      M(t);
    } });
  }, he = (l, Z) => {
    Z = C(Z), E(l, { isVoid: !0, name: Z, fromWireType: () => {
    }, toWireType: (c, d) => {
    } });
  }, Re = (l) => {
    hZ(l, !$, 1, !vl, 65536, !1), I.threadInitTLS();
  }, VZ = (l) => {
    if (Atomics.waitAsync) {
      var Z = Atomics.waitAsync((a(), f), l >> 2, l);
      n(Z.async), Z.value.then(xl);
      var c = l + 128;
      Atomics.store((a(), f), c >> 2, 1);
    }
  }, xl = () => WZ(() => {
    var l = Rl();
    l && (VZ(l), Cc());
  }), oe = (l, Z) => {
    if (l == Z)
      setTimeout(xl);
    else if (p)
      postMessage({ targetThread: l, cmd: "checkMailbox" });
    else {
      var c = I.pthreads[l];
      if (!c) {
        Y(`Cannot send message to thread with ID ${l}, unknown thread ID!`);
        return;
      }
      c.postMessage({ cmd: "checkMailbox" });
    }
  }, Kl = [], se = (l, Z, c, d, b) => {
    d /= 2, Kl.length = d;
    for (var m = b >> 3, t = 0; t < d; t++)
      (a(), B)[m + 2 * t] ? Kl[t] = (a(), B)[m + 2 * t + 1] : Kl[t] = (a(), Yl)[m + 2 * t + 1];
    var i = Z ? yZ[Z] : $e[l];
    n(!(l && Z)), n(i.length == d, "Call args mismatch in _emscripten_receive_on_main_thread_js"), I.currentProxiedOperationCallerThread = c;
    var W = i(...Kl);
    return I.currentProxiedOperationCallerThread = 0, n(typeof W != "bigint"), W;
  }, ue = (l) => {
    p ? postMessage({ cmd: "cleanupThread", thread: l }) : zZ(l);
  }, pe = (l) => {
  }, Fe = 9007199254740992, Ye = -9007199254740992, XZ = (l) => l < Ye || l > Fe ? NaN : Number(l);
  function sc(l, Z, c, d, b, m, t) {
    if (p) return z(10, 0, 1, l, Z, c, d, b, m, t);
    b = XZ(b);
    try {
      n(!isNaN(b));
      var i = H.getStreamFromFD(d), W = e.mmap(i, l, b, Z, c), X = W.ptr;
      return (a(), f)[m >> 2] = W.allocated, (a(), R)[t >> 2] = X, 0;
    } catch (h) {
      if (typeof e > "u" || h.name !== "ErrnoError") throw h;
      return -h.errno;
    }
  }
  function uc(l, Z, c, d, b, m) {
    if (p) return z(11, 0, 1, l, Z, c, d, b, m);
    m = XZ(m);
    try {
      var t = H.getStreamFromFD(b);
      c & 2 && H.doMsync(l, t, Z, d, m);
    } catch (i) {
      if (typeof e > "u" || i.name !== "ErrnoError") throw i;
      return -i.errno;
    }
  }
  var Ie = (l, Z, c, d) => {
    var b = (/* @__PURE__ */ new Date()).getFullYear(), m = new Date(b, 0, 1), t = new Date(b, 6, 1), i = m.getTimezoneOffset(), W = t.getTimezoneOffset(), X = Math.max(i, W);
    (a(), R)[l >> 2] = X * 60, (a(), f)[Z >> 2] = +(i != W);
    var h = (o) => {
      var J = o >= 0 ? "-" : "+", T = Math.abs(o), k = String(Math.floor(T / 60)).padStart(2, "0"), g = String(T % 60).padStart(2, "0");
      return `UTC${J}${k}${g}`;
    }, s = h(i), G = h(W);
    n(s), n(G), n(tl(s) <= 16, `timezone name truncated to fit in TZNAME_MAX (${s})`), n(tl(G) <= 16, `timezone name truncated to fit in TZNAME_MAX (${G})`), W < i ? (D(s, c, 17), D(G, d, 17)) : (D(s, d, 17), D(G, c, 17));
  }, pc = () => performance.timeOrigin + performance.now(), Je = () => Date.now(), Ne = (l) => l >= 0 && l <= 3;
  function ke(l, Z, c) {
    if (!Ne(l))
      return 28;
    var d;
    l === 0 ? d = Je() : d = pc();
    var b = Math.round(d * 1e3 * 1e3);
    return (a(), B)[c >> 3] = BigInt(b), 0;
  }
  var wl = [], ve = (l, Z) => {
    n(Array.isArray(wl)), n(Z % 16 == 0), wl.length = 0;
    for (var c; c = (a(), w)[l++]; ) {
      var d = String.fromCharCode(c), b = ["d", "f", "i", "p"];
      b.push("j"), n(b.includes(d), `Invalid character ${c}("${d}") in readEmAsmArgs! Use only [${b}], and do not specify "v" for void return argument.`);
      var m = c != 105;
      m &= c != 112, Z += m && Z % 8 ? 4 : 0, wl.push(c == 112 ? (a(), R)[Z >> 2] : c == 106 ? (a(), B)[Z >> 3] : c == 105 ? (a(), f)[Z >> 2] : (a(), Yl)[Z >> 3]), Z += m ? 8 : 4;
    }
    return wl;
  }, Ue = (l, Z, c) => {
    var d = ve(Z, c);
    return n(yZ.hasOwnProperty(l), `No EM_ASM constant found at address ${l}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), yZ[l](...d);
  }, He = (l, Z, c) => Ue(l, Z, c), Te = () => {
    $ || ll("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
  }, fe = (l, Z) => Y(_(l, Z)), ge = () => {
    throw aZ(), "unwind";
  }, Fc = () => 2147483648, Be = () => Fc(), ze = () => navigator.hardwareConcurrency, il = {}, Yc = (l) => {
    var Z = tl(l) + 1, c = Nl(Z);
    return c && D(l, c, Z), c;
  }, Cl = (l) => {
    var Z;
    if (Z = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(l))
      return +Z[1];
    if (Z = /\bwasm-function\[(\d+)\]:(\d+)/.exec(l))
      ll("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
    else if (Z = /:(\d+):\d+(?:\)|$)/.exec(l))
      return 2147483648 | +Z[1];
    return 0;
  }, Ic = (l) => {
    for (var Z of l) {
      var c = Cl(Z);
      c && (il[c] = Z);
    }
  }, Jc = () => new Error().stack.toString(), Se = () => {
    var l = Jc().split(`
`);
    return l[0] == "Error" && l.shift(), Ic(l), il.last_addr = Cl(l[3]), il.last_stack = l, il.last_addr;
  }, Ql = (l) => {
    var Z = il[l];
    if (!Z) return 0;
    var c, d;
    if (d = /^\s+at .*\.wasm\.(.*) \(.*\)$/.exec(Z))
      c = d[1];
    else if (d = /^\s+at (.*) \(.*\)$/.exec(Z))
      c = d[1];
    else if (d = /^(.+?)@/.exec(Z))
      c = d[1];
    else
      return 0;
    return M(Ql.ret ?? 0), Ql.ret = Yc(c), Ql.ret;
  }, xe = (l) => {
    var Z = L.buffer.byteLength, c = (l - Z + 65535) / 65536 | 0;
    try {
      return L.grow(c), Tl(), 1;
    } catch (d) {
      Y(`growMemory: Attempted to grow heap from ${Z} bytes to ${l} bytes, but got error: ${d}`);
    }
  }, Ke = (l) => {
    var Z = (a(), w).length;
    if (l >>>= 0, l <= Z)
      return !1;
    var c = Fc();
    if (l > c)
      return Y(`Cannot enlarge memory, requested ${l} bytes, but the limit is ${c} bytes!`), !1;
    for (var d = 1; d <= 4; d *= 2) {
      var b = Z * (1 + 0.2 / d);
      b = Math.min(b, l + 100663296);
      var m = Math.min(c, $Z(Math.max(l, b), 65536)), t = xe(m);
      if (t)
        return !0;
    }
    return Y(`Failed to grow the heap from ${Z} bytes to ${m} bytes, not enough memory!`), !1;
  }, we = (l, Z, c) => {
    var d;
    il.last_addr == l ? d = il.last_stack : (d = Jc().split(`
`), d[0] == "Error" && d.shift(), Ic(d));
    for (var b = 3; d[b] && Cl(d[b]) != l; )
      ++b;
    for (var m = 0; m < c && d[m + b]; ++m)
      (a(), f)[Z + m * 4 >> 2] = Cl(d[m + b]);
    return m;
  }, rZ = {}, Ce = () => pZ || "./this.program", Jl = () => {
    if (!Jl.strings) {
      var l = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", Z = { USER: "web_user", LOGNAME: "web_user", PATH: "/", PWD: "/", HOME: "/home/web_user", LANG: l, _: Ce() };
      for (var c in rZ)
        rZ[c] === void 0 ? delete Z[c] : Z[c] = rZ[c];
      var d = [];
      for (var c in Z)
        d.push(`${c}=${Z[c]}`);
      Jl.strings = d;
    }
    return Jl.strings;
  };
  function Nc(l, Z) {
    if (p) return z(12, 0, 1, l, Z);
    var c = 0, d = 0;
    for (var b of Jl()) {
      var m = Z + c;
      (a(), R)[l + d >> 2] = m, c += D(b, m, 1 / 0) + 1, d += 4;
    }
    return 0;
  }
  function kc(l, Z) {
    if (p) return z(13, 0, 1, l, Z);
    var c = Jl();
    (a(), R)[l >> 2] = c.length;
    var d = 0;
    for (var b of c)
      d += tl(b) + 1;
    return (a(), R)[Z >> 2] = d, 0;
  }
  function vc(l) {
    if (p) return z(14, 0, 1, l);
    try {
      var Z = H.getStreamFromFD(l);
      return e.close(Z), 0;
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return c.errno;
    }
  }
  var Qe = (l, Z, c, d) => {
    for (var b = 0, m = 0; m < c; m++) {
      var t = (a(), R)[Z >> 2], i = (a(), R)[Z + 4 >> 2];
      Z += 8;
      var W = e.read(l, (a(), S), t, i, d);
      if (W < 0) return -1;
      if (b += W, W < i) break;
    }
    return b;
  };
  function Uc(l, Z, c, d) {
    if (p) return z(15, 0, 1, l, Z, c, d);
    try {
      var b = H.getStreamFromFD(l), m = Qe(b, Z, c);
      return (a(), R)[d >> 2] = m, 0;
    } catch (t) {
      if (typeof e > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  function Hc(l, Z, c, d) {
    if (p) return z(16, 0, 1, l, Z, c, d);
    Z = XZ(Z);
    try {
      if (isNaN(Z)) return 61;
      var b = H.getStreamFromFD(l);
      return e.llseek(b, Z, c), (a(), B)[d >> 3] = BigInt(b.position), b.getdents && Z === 0 && c === 0 && (b.getdents = null), 0;
    } catch (m) {
      if (typeof e > "u" || m.name !== "ErrnoError") throw m;
      return m.errno;
    }
  }
  var Le = (l, Z, c, d) => {
    for (var b = 0, m = 0; m < c; m++) {
      var t = (a(), R)[Z >> 2], i = (a(), R)[Z + 4 >> 2];
      Z += 8;
      var W = e.write(l, (a(), S), t, i, d);
      if (W < 0) return -1;
      if (b += W, W < i)
        break;
    }
    return b;
  };
  function Tc(l, Z, c, d) {
    if (p) return z(17, 0, 1, l, Z, c, d);
    try {
      var b = H.getStreamFromFD(l), m = Le(b, Z, c);
      return (a(), R)[d >> 2] = m, 0;
    } catch (t) {
      if (typeof e > "u" || t.name !== "ErrnoError") throw t;
      return t.errno;
    }
  }
  function Ee(l, Z) {
    try {
      return bZ((a(), w).subarray(l, l + Z)), 0;
    } catch (c) {
      if (typeof e > "u" || c.name !== "ErrnoError") throw c;
      return c.errno;
    }
  }
  var Me = (l) => {
    var Z = V["_" + l];
    return n(Z, "Cannot call unknown function " + l + ", make sure it is exported"), Z;
  }, je = (l, Z) => {
    n(l.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (a(), S).set(l, Z);
  }, Pe = (l) => {
    var Z = tl(l) + 1, c = ZZ(Z);
    return D(l, c, Z), c;
  }, fc = (l, Z, c, d, b) => {
    var m = { string: (k) => {
      var g = 0;
      return k != null && k !== 0 && (g = Pe(k)), g;
    }, array: (k) => {
      var g = ZZ(k.length);
      return je(k, g), g;
    } };
    function t(k) {
      return Z === "string" ? _(k) : Z === "boolean" ? !!k : k;
    }
    var i = Me(l), W = [], X = 0;
    if (n(Z !== "array", 'Return type should not be "array".'), d)
      for (var h = 0; h < d.length; h++) {
        var s = m[c[h]];
        s ? (X === 0 && (X = LZ()), W[h] = s(d[h])) : W[h] = d[h];
      }
    var G = r.currData, o = i(...W);
    function J(k) {
      return hc(), X !== 0 && lZ(X), t(k);
    }
    var T = b?.async;
    return aZ(), r.currData != G ? (n(!(G && r.currData), "We cannot start an async operation when one is already flight"), n(!(G && !r.currData), "We cannot stop an async operation in flight"), n(T, "The call to " + l + " is running asynchronously. If this was intended, add the async option to the ccall/cwrap call."), r.whenDone().then(J)) : (o = J(o), T ? Promise.resolve(o) : o);
  }, _e = (l, Z, c, d) => (...b) => fc(l, Z, c, b, d), Oe = (...l) => Yc(...l);
  I.init(), e.createPreloadedFile = Hd, e.preloadFile = cc, e.staticInit(), n(O.length === 10);
  {
    if (ed(), V.noExitRuntime && (eZ = V.noExitRuntime), V.preloadPlugins && (Zc = V.preloadPlugins), V.print && (q = V.print), V.printErr && (Y = V.printErr), V.wasmBinary && (ul = V.wasmBinary), qe(), V.arguments && V.arguments, V.thisProgram && (pZ = V.thisProgram), n(typeof V.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), n(typeof V.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), n(typeof V.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), n(typeof V.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), n(typeof V.read > "u", "Module.read option was removed"), n(typeof V.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), n(typeof V.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), n(typeof V.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), n(typeof V.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), n(typeof V.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), n(typeof V.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), V.preInit)
      for (typeof V.preInit == "function" && (V.preInit = [V.preInit]); V.preInit.length > 0; )
        V.preInit.shift()();
    Hl("preInit");
  }
  V.ccall = fc, V.cwrap = _e, V.UTF8ToString = _, V.stringToUTF8 = D, V.allocateUTF8 = Oe;
  var De = ["writeI53ToI64", "writeI53ToI64Clamped", "writeI53ToI64Signaling", "writeI53ToU64Clamped", "writeI53ToU64Signaling", "readI53FromI64", "readI53FromU64", "convertI32PairToI53", "convertI32PairToI53Checked", "convertU32PairToI53", "getTempRet0", "setTempRet0", "withStackSave", "inetPton4", "inetNtop4", "inetPton6", "inetNtop6", "readSockaddr", "writeSockaddr", "runMainThreadEmAsm", "jstoi_q", "autoResumeAudioContext", "asmjsMangle", "HandleAllocator", "addOnInit", "addOnPostCtor", "addOnPreMain", "addOnExit", "STACK_SIZE", "STACK_ALIGN", "POINTER_SIZE", "ASSERTIONS", "convertJsFunctionToWasm", "getEmptyTableSlot", "updateTableMap", "getFunctionAddress", "addFunction", "removeFunction", "intArrayToString", "stringToAscii", "registerKeyEventCallback", "maybeCStringToJsString", "findEventTarget", "getBoundingClientRect", "fillMouseEventData", "registerMouseEventCallback", "registerWheelEventCallback", "registerUiEventCallback", "registerFocusEventCallback", "fillDeviceOrientationEventData", "registerDeviceOrientationEventCallback", "fillDeviceMotionEventData", "registerDeviceMotionEventCallback", "screenOrientation", "fillOrientationChangeEventData", "registerOrientationChangeEventCallback", "fillFullscreenChangeEventData", "registerFullscreenChangeEventCallback", "JSEvents_requestFullscreen", "JSEvents_resizeCanvasForFullscreen", "registerRestoreOldStyle", "hideEverythingExceptGivenElement", "restoreHiddenElements", "setLetterbox", "softFullscreenResizeWebGLRenderTarget", "doRequestFullscreen", "fillPointerlockChangeEventData", "registerPointerlockChangeEventCallback", "registerPointerlockErrorEventCallback", "requestPointerLock", "fillVisibilityChangeEventData", "registerVisibilityChangeEventCallback", "registerTouchEventCallback", "fillGamepadEventData", "registerGamepadEventCallback", "registerBeforeUnloadEventCallback", "fillBatteryEventData", "registerBatteryEventCallback", "setCanvasElementSizeCallingThread", "setCanvasElementSizeMainThread", "setCanvasElementSize", "getCanvasSizeCallingThread", "getCanvasSizeMainThread", "getCanvasElementSize", "getCallstack", "convertPCtoSourceLocation", "wasiRightsToMuslOFlags", "wasiOFlagsToMuslOFlags", "safeSetTimeout", "setImmediateWrapped", "safeRequestAnimationFrame", "clearImmediateWrapped", "registerPostMainLoop", "registerPreMainLoop", "getPromise", "makePromise", "idsToPromises", "makePromiseCallback", "findMatchingCatch", "Browser_asyncPrepareDataCounter", "isLeapYear", "ydayFromDate", "arraySum", "addDays", "getSocketFromFD", "getSocketAddress", "FS_mkdirTree", "_setNetworkCallback", "heapObjectForWebGLType", "toTypedArrayIndex", "webgl_enable_ANGLE_instanced_arrays", "webgl_enable_OES_vertex_array_object", "webgl_enable_WEBGL_draw_buffers", "webgl_enable_WEBGL_multi_draw", "webgl_enable_EXT_polygon_offset_clamp", "webgl_enable_EXT_clip_control", "webgl_enable_WEBGL_polygon_mode", "emscriptenWebGLGet", "computeUnpackAlignedImageSize", "colorChannelsInGlTextureFormat", "emscriptenWebGLGetTexPixelData", "emscriptenWebGLGetUniform", "webglGetUniformLocation", "webglPrepareUniformLocationsBeforeFirstUse", "webglGetLeftBracePos", "emscriptenWebGLGetVertexAttrib", "__glGetActiveAttribOrUniform", "writeGLArray", "emscripten_webgl_destroy_context_before_on_calling_thread", "registerWebGlEventCallback", "ALLOC_NORMAL", "ALLOC_STACK", "allocate", "writeStringToMemory", "writeAsciiToMemory", "allocateUTF8OnStack", "demangle", "stackTrace", "getNativeTypeSize", "getFunctionArgsName", "requireRegisteredType", "createJsInvokerSignature", "PureVirtualError", "getBasestPointer", "registerInheritedInstance", "unregisterInheritedInstance", "getInheritedInstance", "getInheritedInstanceCount", "getLiveInheritedInstances", "enumReadValueFromPointer", "genericPointerToWireType", "constNoSmartPtrRawPointerToWireType", "nonConstNoSmartPtrRawPointerToWireType", "init_RegisteredPointer", "RegisteredPointer", "RegisteredPointer_fromWireType", "runDestructor", "releaseClassHandle", "detachFinalizer", "attachFinalizer", "makeClassHandle", "init_ClassHandle", "ClassHandle", "throwInstanceAlreadyDeleted", "flushPendingDeletes", "setDelayFunction", "RegisteredClass", "shallowCopyInternalPointer", "downcastPointer", "upcastPointer", "validateThis", "char_0", "char_9", "makeLegalFunctionName", "count_emval_handles", "getStringOrSymbol", "emval_returnValue", "emval_lookupTypes", "emval_addMethodCaller"];
  De.forEach(cd);
  var Ae = ["run", "out", "err", "callMain", "abort", "wasmExports", "HEAPF32", "HEAPF64", "HEAP8", "HEAP16", "HEAPU16", "HEAP32", "HEAPU32", "HEAP64", "HEAPU64", "writeStackCookie", "checkStackCookie", "INT53_MAX", "INT53_MIN", "bigintToI53Checked", "stackSave", "stackRestore", "stackAlloc", "createNamedFunction", "ptrToString", "zeroMemory", "exitJS", "getHeapMax", "growMemory", "ENV", "ERRNO_CODES", "strError", "DNS", "Protocols", "Sockets", "timers", "warnOnce", "readEmAsmArgsArray", "readEmAsmArgs", "runEmAsmFunction", "getExecutableName", "dynCallLegacy", "getDynCaller", "dynCall", "handleException", "keepRuntimeAlive", "runtimeKeepalivePush", "runtimeKeepalivePop", "callUserCallback", "maybeExit", "asyncLoad", "alignMemory", "mmapAlloc", "wasmTable", "wasmMemory", "getUniqueRunDependency", "noExitRuntime", "addRunDependency", "removeRunDependency", "addOnPreRun", "addOnPostRun", "freeTableIndexes", "functionsInTableMap", "setValue", "getValue", "PATH", "PATH_FS", "UTF8Decoder", "UTF8ArrayToString", "stringToUTF8Array", "lengthBytesUTF8", "intArrayFromString", "AsciiToString", "UTF16Decoder", "UTF16ToString", "stringToUTF16", "lengthBytesUTF16", "UTF32ToString", "stringToUTF32", "lengthBytesUTF32", "stringToNewUTF8", "stringToUTF8OnStack", "writeArrayToMemory", "JSEvents", "specialHTMLTargets", "findCanvasEventTarget", "currentFullscreenStrategy", "restoreOldWindowedStyle", "jsStackTrace", "UNWIND_CACHE", "ExitStatus", "getEnvStrings", "checkWasiClock", "doReadv", "doWritev", "initRandomFill", "randomFill", "emSetImmediate", "emClearImmediate_deps", "emClearImmediate", "promiseMap", "uncaughtExceptionCount", "exceptionLast", "exceptionCaught", "ExceptionInfo", "Browser", "requestFullscreen", "requestFullScreen", "setCanvasSize", "getUserMedia", "createContext", "getPreloadedImageData__data", "wget", "MONTH_DAYS_REGULAR", "MONTH_DAYS_LEAP", "MONTH_DAYS_REGULAR_CUMULATIVE", "MONTH_DAYS_LEAP_CUMULATIVE", "SYSCALLS", "preloadPlugins", "FS_createPreloadedFile", "FS_preloadFile", "FS_modeStringToFlags", "FS_getMode", "FS_stdin_getChar_buffer", "FS_stdin_getChar", "FS_unlink", "FS_createPath", "FS_createDevice", "FS_readFile", "FS", "FS_root", "FS_mounts", "FS_devices", "FS_streams", "FS_nextInode", "FS_nameTable", "FS_currentPath", "FS_initialized", "FS_ignorePermissions", "FS_filesystems", "FS_syncFSRequests", "FS_readFiles", "FS_lookupPath", "FS_getPath", "FS_hashName", "FS_hashAddNode", "FS_hashRemoveNode", "FS_lookupNode", "FS_createNode", "FS_destroyNode", "FS_isRoot", "FS_isMountpoint", "FS_isFile", "FS_isDir", "FS_isLink", "FS_isChrdev", "FS_isBlkdev", "FS_isFIFO", "FS_isSocket", "FS_flagsToPermissionString", "FS_nodePermissions", "FS_mayLookup", "FS_mayCreate", "FS_mayDelete", "FS_mayOpen", "FS_checkOpExists", "FS_nextfd", "FS_getStreamChecked", "FS_getStream", "FS_createStream", "FS_closeStream", "FS_dupStream", "FS_doSetAttr", "FS_chrdev_stream_ops", "FS_major", "FS_minor", "FS_makedev", "FS_registerDevice", "FS_getDevice", "FS_getMounts", "FS_syncfs", "FS_mount", "FS_unmount", "FS_lookup", "FS_mknod", "FS_statfs", "FS_statfsStream", "FS_statfsNode", "FS_create", "FS_mkdir", "FS_mkdev", "FS_symlink", "FS_rename", "FS_rmdir", "FS_readdir", "FS_readlink", "FS_stat", "FS_fstat", "FS_lstat", "FS_doChmod", "FS_chmod", "FS_lchmod", "FS_fchmod", "FS_doChown", "FS_chown", "FS_lchown", "FS_fchown", "FS_doTruncate", "FS_truncate", "FS_ftruncate", "FS_utime", "FS_open", "FS_close", "FS_isClosed", "FS_llseek", "FS_read", "FS_write", "FS_mmap", "FS_msync", "FS_ioctl", "FS_writeFile", "FS_cwd", "FS_chdir", "FS_createDefaultDirectories", "FS_createDefaultDevices", "FS_createSpecialDirectories", "FS_createStandardStreams", "FS_staticInit", "FS_init", "FS_quit", "FS_findObject", "FS_analyzePath", "FS_createFile", "FS_createDataFile", "FS_forceLoadFile", "FS_createLazyFile", "FS_absolutePath", "FS_createFolder", "FS_createLink", "FS_joinPath", "FS_mmapAlloc", "FS_standardizePath", "MEMFS", "TTY", "PIPEFS", "SOCKFS", "tempFixedLengthArray", "miniTempWebGLFloatBuffers", "miniTempWebGLIntBuffers", "GL", "AL", "GLUT", "EGL", "GLEW", "IDBStore", "runAndAbortIfError", "Asyncify", "Fibers", "SDL", "SDL_gfx", "print", "printErr", "jstoi_s", "PThread", "terminateWorker", "cleanupThread", "registerTLSInit", "spawnThread", "exitOnMainThread", "proxyToMainThread", "proxiedJSCallArgs", "invokeEntryPoint", "checkMailbox", "InternalError", "BindingError", "throwInternalError", "throwBindingError", "registeredTypes", "awaitingDependencies", "typeDependencies", "tupleRegistrations", "structRegistrations", "sharedRegisterType", "whenDependentTypesAreResolved", "getTypeName", "getFunctionName", "heap32VectorToArray", "usesDestructorStack", "checkArgCount", "getRequiredArgCount", "createJsInvoker", "UnboundTypeError", "EmValType", "EmValOptionalType", "throwUnboundTypeError", "ensureOverloadTable", "exposePublicSymbol", "replacePublicSymbol", "embindRepr", "registeredInstances", "registeredPointers", "registerType", "integerReadValueFromPointer", "floatReadValueFromPointer", "assertIntegerRange", "readPointer", "runDestructors", "craftInvokerFunction", "embind__requireFunction", "finalizationRegistry", "detachFinalizer_deps", "deletionQueue", "delayFunction", "emval_freelist", "emval_handles", "emval_symbols", "Emval", "emval_methodCallers"];
  Ae.forEach(JZ);
  var $e = [cZ, EZ, OZ, dc, ec, bc, mc, tc, ic, nc, sc, uc, Nc, kc, vc, Uc, Hc, Tc];
  function qe() {
    ld("fetchSettings");
  }
  var yZ = { 540932: () => typeof wasmOffsetConverter < "u" };
  function lb() {
    return typeof wasmOffsetConverter < "u";
  }
  var gc = N("___getTypeName"), Bc = N("__embind_initialize_bindings");
  V._get_cp_model_schema = N("_get_cp_model_schema"), V._get_sat_parameters_schema = N("_get_sat_parameters_schema"), V._solve_model = N("_solve_model");
  var Nl = V._malloc = N("_malloc");
  V._free_buffer = N("_free_buffer");
  var M = V._free = N("_free");
  V._interrupt_solve = N("_interrupt_solve"), V._validate_model = N("_validate_model");
  var Rl = N("_pthread_self"), GZ = N("_fflush"), zc = N("_strerror"), Sc = N("_emscripten_builtin_memalign"), hZ = N("__emscripten_thread_init"), xc = N("__emscripten_thread_crashed"), RZ = N("_emscripten_stack_get_end"), Kc = N("__emscripten_run_js_on_main_thread"), wc = N("__emscripten_thread_free_data"), oZ = N("__emscripten_thread_exit"), Cc = N("__emscripten_check_mailbox"), Qc = N("_emscripten_stack_init"), Lc = N("_emscripten_stack_set_limits"), Ec = N("__emscripten_stack_restore"), Mc = N("__emscripten_stack_alloc"), sZ = N("_emscripten_stack_get_current"), jc = N("dynCall_ii"), Pc = N("_asyncify_start_unwind"), _c = N("_asyncify_stop_unwind"), Oc = N("_asyncify_start_rewind"), Dc = N("_asyncify_stop_rewind");
  function Zb(l) {
    n(typeof l.__getTypeName < "u", "missing Wasm export: __getTypeName"), n(typeof l._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), n(typeof l.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), n(typeof l.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), n(typeof l.solve_model < "u", "missing Wasm export: solve_model"), n(typeof l.malloc < "u", "missing Wasm export: malloc"), n(typeof l.free_buffer < "u", "missing Wasm export: free_buffer"), n(typeof l.free < "u", "missing Wasm export: free"), n(typeof l.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), n(typeof l.validate_model < "u", "missing Wasm export: validate_model"), n(typeof l.pthread_self < "u", "missing Wasm export: pthread_self"), n(typeof l.fflush < "u", "missing Wasm export: fflush"), n(typeof l.strerror < "u", "missing Wasm export: strerror"), n(typeof l._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), n(typeof l.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), n(typeof l._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), n(typeof l._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), n(typeof l.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), n(typeof l.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), n(typeof l._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), n(typeof l._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), n(typeof l._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), n(typeof l._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), n(typeof l.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), n(typeof l.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), n(typeof l.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), n(typeof l._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), n(typeof l._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), n(typeof l.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), n(typeof l.dynCall_ii < "u", "missing Wasm export: dynCall_ii"), n(typeof l.dynCall_v < "u", "missing Wasm export: dynCall_v"), n(typeof l.dynCall_vi < "u", "missing Wasm export: dynCall_vi"), n(typeof l.dynCall_iii < "u", "missing Wasm export: dynCall_iii"), n(typeof l.dynCall_iiii < "u", "missing Wasm export: dynCall_iiii"), n(typeof l.dynCall_vii < "u", "missing Wasm export: dynCall_vii"), n(typeof l.dynCall_iiiijij < "u", "missing Wasm export: dynCall_iiiijij"), n(typeof l.dynCall_viii < "u", "missing Wasm export: dynCall_viii"), n(typeof l.dynCall_iiiii < "u", "missing Wasm export: dynCall_iiiii"), n(typeof l.dynCall_viiii < "u", "missing Wasm export: dynCall_viiii"), n(typeof l.dynCall_viiiiii < "u", "missing Wasm export: dynCall_viiiiii"), n(typeof l.dynCall_viiiii < "u", "missing Wasm export: dynCall_viiiii"), n(typeof l.dynCall_viiiij < "u", "missing Wasm export: dynCall_viiiij"), n(typeof l.dynCall_ji < "u", "missing Wasm export: dynCall_ji"), n(typeof l.dynCall_viij < "u", "missing Wasm export: dynCall_viij"), n(typeof l.dynCall_i < "u", "missing Wasm export: dynCall_i"), n(typeof l.dynCall_di < "u", "missing Wasm export: dynCall_di"), n(typeof l.dynCall_viijiiii < "u", "missing Wasm export: dynCall_viijiiii"), n(typeof l.dynCall_jii < "u", "missing Wasm export: dynCall_jii"), n(typeof l.dynCall_viji < "u", "missing Wasm export: dynCall_viji"), n(typeof l.dynCall_vifi < "u", "missing Wasm export: dynCall_vifi"), n(typeof l.dynCall_vidi < "u", "missing Wasm export: dynCall_vidi"), n(typeof l.dynCall_viiiiiii < "u", "missing Wasm export: dynCall_viiiiiii"), n(typeof l.dynCall_iiiiiii < "u", "missing Wasm export: dynCall_iiiiiii"), n(typeof l.dynCall_viiji < "u", "missing Wasm export: dynCall_viiji"), n(typeof l.dynCall_jiiji < "u", "missing Wasm export: dynCall_jiiji"), n(typeof l.dynCall_dij < "u", "missing Wasm export: dynCall_dij"), n(typeof l.dynCall_dii < "u", "missing Wasm export: dynCall_dii"), n(typeof l.dynCall_fii < "u", "missing Wasm export: dynCall_fii"), n(typeof l.dynCall_viijii < "u", "missing Wasm export: dynCall_viijii"), n(typeof l.dynCall_iiiiii < "u", "missing Wasm export: dynCall_iiiiii"), n(typeof l.dynCall_vj < "u", "missing Wasm export: dynCall_vj"), n(typeof l.dynCall_vij < "u", "missing Wasm export: dynCall_vij"), n(typeof l.dynCall_jiji < "u", "missing Wasm export: dynCall_jiji"), n(typeof l.dynCall_iidiiii < "u", "missing Wasm export: dynCall_iidiiii"), n(typeof l.dynCall_iiiiiiiii < "u", "missing Wasm export: dynCall_iiiiiiiii"), n(typeof l.dynCall_iiiiij < "u", "missing Wasm export: dynCall_iiiiij"), n(typeof l.dynCall_iiiiid < "u", "missing Wasm export: dynCall_iiiiid"), n(typeof l.dynCall_iiiiijj < "u", "missing Wasm export: dynCall_iiiiijj"), n(typeof l.dynCall_iiiiiiii < "u", "missing Wasm export: dynCall_iiiiiiii"), n(typeof l.dynCall_iiiiiijj < "u", "missing Wasm export: dynCall_iiiiiijj"), n(typeof l.asyncify_start_unwind < "u", "missing Wasm export: asyncify_start_unwind"), n(typeof l.asyncify_stop_unwind < "u", "missing Wasm export: asyncify_stop_unwind"), n(typeof l.asyncify_start_rewind < "u", "missing Wasm export: asyncify_start_rewind"), n(typeof l.asyncify_stop_rewind < "u", "missing Wasm export: asyncify_stop_rewind"), n(typeof l.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), gc = y("__getTypeName", 1), Bc = y("_embind_initialize_bindings", 0), V._get_cp_model_schema = y("get_cp_model_schema", 0), V._get_sat_parameters_schema = y("get_sat_parameters_schema", 0), V._solve_model = y("solve_model", 5), Nl = V._malloc = y("malloc", 1), V._free_buffer = y("free_buffer", 1), M = V._free = y("free", 1), V._interrupt_solve = y("interrupt_solve", 0), V._validate_model = y("validate_model", 3), Rl = y("pthread_self", 0), GZ = y("fflush", 1), zc = y("strerror", 1), Sc = y("emscripten_builtin_memalign", 2), hZ = y("_emscripten_thread_init", 6), xc = y("_emscripten_thread_crashed", 0), RZ = l.emscripten_stack_get_end, l.emscripten_stack_get_base, Kc = y("_emscripten_run_js_on_main_thread", 5), wc = y("_emscripten_thread_free_data", 1), oZ = y("_emscripten_thread_exit", 1), Cc = y("_emscripten_check_mailbox", 0), Qc = l.emscripten_stack_init, Lc = l.emscripten_stack_set_limits, l.emscripten_stack_get_free, Ec = l._emscripten_stack_restore, Mc = l._emscripten_stack_alloc, sZ = l.emscripten_stack_get_current, jc = u.ii = y("dynCall_ii", 2), u.v = y("dynCall_v", 1), u.vi = y("dynCall_vi", 2), u.iii = y("dynCall_iii", 3), u.iiii = y("dynCall_iiii", 4), u.vii = y("dynCall_vii", 3), u.iiiijij = y("dynCall_iiiijij", 7), u.viii = y("dynCall_viii", 4), u.iiiii = y("dynCall_iiiii", 5), u.viiii = y("dynCall_viiii", 5), u.viiiiii = y("dynCall_viiiiii", 7), u.viiiii = y("dynCall_viiiii", 6), u.viiiij = y("dynCall_viiiij", 6), u.ji = y("dynCall_ji", 2), u.viij = y("dynCall_viij", 4), u.i = y("dynCall_i", 1), u.di = y("dynCall_di", 2), u.viijiiii = y("dynCall_viijiiii", 8), u.jii = y("dynCall_jii", 3), u.viji = y("dynCall_viji", 4), u.vifi = y("dynCall_vifi", 4), u.vidi = y("dynCall_vidi", 4), u.viiiiiii = y("dynCall_viiiiiii", 8), u.iiiiiii = y("dynCall_iiiiiii", 7), u.viiji = y("dynCall_viiji", 5), u.jiiji = y("dynCall_jiiji", 5), u.dij = y("dynCall_dij", 3), u.dii = y("dynCall_dii", 3), u.fii = y("dynCall_fii", 3), u.viijii = y("dynCall_viijii", 6), u.iiiiii = y("dynCall_iiiiii", 6), u.vj = y("dynCall_vj", 2), u.vij = y("dynCall_vij", 3), u.jiji = y("dynCall_jiji", 4), u.iidiiii = y("dynCall_iidiiii", 7), u.iiiiiiiii = y("dynCall_iiiiiiiii", 9), u.iiiiij = y("dynCall_iiiiij", 6), u.iiiiid = y("dynCall_iiiiid", 6), u.iiiiijj = y("dynCall_iiiiijj", 7), u.iiiiiiii = y("dynCall_iiiiiiii", 8), u.iiiiiijj = y("dynCall_iiiiiijj", 8), Pc = y("asyncify_start_unwind", 1), _c = y("asyncify_stop_unwind", 0), Oc = y("asyncify_start_rewind", 1), Dc = y("asyncify_stop_rewind", 0), l.__indirect_function_table;
  }
  var ol;
  function cb() {
    ol = { HaveOffsetConverter: lb, __assert_fail: Rd, __cxa_throw: sd, __pthread_create_js: DZ, __syscall_fcntl64: dc, __syscall_fstat64: ec, __syscall_ioctl: bc, __syscall_lstat64: mc, __syscall_newfstatat: tc, __syscall_openat: ic, __syscall_stat64: nc, _abort_js: Td, _embind_register_bigint: Bd, _embind_register_bool: zd, _embind_register_emval: Kd, _embind_register_float: Cd, _embind_register_function: me, _embind_register_integer: te, _embind_register_memory_view: ie, _embind_register_std_string: ne, _embind_register_std_wstring: Ge, _embind_register_void: he, _emscripten_init_main_thread_js: Re, _emscripten_notify_mailbox_postmessage: oe, _emscripten_receive_on_main_thread_js: se, _emscripten_thread_cleanup: ue, _emscripten_thread_mailbox_await: VZ, _emscripten_thread_set_strongref: pe, _mmap_js: sc, _munmap_js: uc, _tzset_js: Ie, clock_time_get: ke, emscripten_asm_const_int: He, emscripten_check_blocking_allowed: Te, emscripten_errn: fe, emscripten_exit_with_live_runtime: ge, emscripten_get_heap_max: Be, emscripten_get_now: pc, emscripten_num_logical_cores: ze, emscripten_pc_get_function: Ql, emscripten_resize_heap: Ke, emscripten_stack_snapshot: Se, emscripten_stack_unwind_buffer: we, environ_get: Nc, environ_sizes_get: kc, exit: dZ, fd_close: vc, fd_read: Uc, fd_seek: Hc, fd_write: Tc, memory: L, proc_exit: cZ, random_get: Ee };
  }
  var Ac;
  function db() {
    n(!p), Qc(), IZ();
  }
  function Ll() {
    if (el > 0) {
      Il = Ll;
      return;
    }
    if (p) {
      Dl?.(V), HZ();
      return;
    }
    if (db(), bd(), el > 0) {
      Il = Ll;
      return;
    }
    function l() {
      n(!Ac), Ac = !0, V.calledRun = !0, !P && (HZ(), Dl?.(V), V.onRuntimeInitialized?.(), Hl("onRuntimeInitialized"), n(!V._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), md());
    }
    V.setStatus ? (V.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => V.setStatus(""), 1), l();
    }, 1)) : l(), pl();
  }
  function eb() {
    var l = q, Z = Y, c = !1;
    q = Y = (W) => {
      c = !0;
    };
    try {
      GZ(0);
      for (var d of ["stdout", "stderr"]) {
        var b = e.analyzePath("/dev/" + d);
        if (!b) return;
        var m = b.object, t = m.rdev, i = Zl.ttys[t];
        i?.output?.length && (c = !0);
      }
    } catch {
    }
    q = l, Y = Z, c && ll("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
  }
  var A;
  p || (A = await fZ(), Ll()), Wl ? Ml = V : Ml = new Promise((l, Z) => {
    Dl = l, Al = Z;
  });
  for (const l of Object.keys(V))
    l in El || Object.defineProperty(El, l, { configurable: !0, get() {
      v(`Access to module property ('${l}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
    } });
  return Ml;
}
var mb = globalThis.self?.name?.startsWith("em-pthread");
mb && bb();
export {
  bb as default
};
//# sourceMappingURL=cp_sat_runtime_asyncify-Bt3WG6KD.js.map
