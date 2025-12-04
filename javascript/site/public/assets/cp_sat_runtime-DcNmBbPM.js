(async ()=>{
    var tn = Object.freeze({
        __proto__: null
    });
    async function co(qe = {}) {
        var Ye;
        (function() {
            function e(d) {
                d = d.split("-")[0];
                for(var _ = d.split(".").slice(0, 3); _.length < 3;)_.push("00");
                return _ = _.map((v, g, m)=>v.padStart(2, "0")), _.join("");
            }
            var r = (d)=>[
                    d / 1e4 | 0,
                    (d / 100 | 0) % 100,
                    d % 100
                ].join("."), t = 2147483647, n = typeof process < "u" && process.versions?.node ? e(process.versions.node) : t;
            if (n < 160400) throw new Error(`This emscripten-generated code requires node v${r(160400)} (detected v${r(n)})`);
            var o = typeof navigator < "u" && navigator.userAgent;
            if (o) {
                var a = o.includes("Safari/") && o.match(/Version\/(\d+\.?\d*\.?\d*)/) ? e(o.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : t;
                if (a < 15e4) throw new Error(`This emscripten-generated code requires Safari v${r(15e4)} (detected v${a})`);
                var s = o.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(o.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : t;
                if (s < 114) throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${s})`);
                var l = o.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(o.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : t;
                if (l < 85) throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${l})`);
            }
        })();
        var f = qe, Me = !!globalThis.window, j = !!globalThis.WorkerGlobalScope, R = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", Tr = !Me && !R && !j, y = j && self.name?.startsWith("em-pthread");
        if (y && (c(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0), R) {
            const { createRequire: e } = await Promise.resolve().then(function() {
                return tn;
            });
            var K = e(import.meta.url), De = K("worker_threads");
            global.Worker = De.Worker, j = !De.isMainThread, y = j && De.workerData == "em-pthread";
        }
        var Je = "./this.program", Ze = (e, r)=>{
            throw r;
        }, Qe = import.meta.url, Re = "";
        function on(e) {
            return f.locateFile ? f.locateFile(e, Re) : Re + e;
        }
        var Ne, be;
        if (R) {
            if (!(globalThis.process?.versions?.node && globalThis.process?.type != "renderer")) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
            var Se = K("fs");
            Qe.startsWith("file:") && (Re = K("path").dirname(K("url").fileURLToPath(Qe)) + "/"), be = (r)=>{
                r = Oe(r) ? new URL(r) : r;
                var t = Se.readFileSync(r);
                return c(Buffer.isBuffer(t)), t;
            }, Ne = async (r, t = !0)=>{
                r = Oe(r) ? new URL(r) : r;
                var n = Se.readFileSync(r, t ? void 0 : "utf8");
                return c(t ? Buffer.isBuffer(n) : typeof n == "string"), n;
            }, process.argv.length > 1 && (Je = process.argv[1].replace(/\\/g, "/")), process.argv.slice(2), Ze = (r, t)=>{
                throw process.exitCode = r, t;
            };
        } else if (!Tr) if (Me || j) {
            try {
                Re = new URL(".", Qe).href;
            } catch  {}
            if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
            R || (j && (be = (e)=>{
                var r = new XMLHttpRequest();
                return r.open("GET", e, !1), r.responseType = "arraybuffer", r.send(null), new Uint8Array(r.response);
            }), Ne = async (e)=>{
                c(!Oe(e), "readAsync does not work with file:// URLs");
                var r = await fetch(e, {
                    credentials: "same-origin"
                });
                if (r.ok) return r.arrayBuffer();
                throw new Error(r.status + " : " + r.url);
            });
        } else throw new Error("environment detection error");
        var Fr = console.log.bind(console), Ar = console.error.bind(console);
        if (R) {
            var an = K("util"), Pr = (e)=>typeof e == "object" ? an.inspect(e) : e;
            Fr = (...e)=>Se.writeSync(1, e.map(Pr).join(" ") + `
`), Ar = (...e)=>Se.writeSync(2, e.map(Pr).join(" ") + `
`);
        }
        var Q = Fr, b = Ar;
        c(Me || j || R, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), c(!Tr, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
        var ke;
        globalThis.WebAssembly || b("no native wasm support detected");
        var ne, de = !1, ce;
        function c(e, r) {
            e || A("Assertion failed" + (r ? ": " + r : ""));
        }
        var Oe = (e)=>e.startsWith("file://");
        function Cr() {
            var e = br();
            c((e & 3) == 0), e == 0 && (e += 4), (u(), h)[e >> 2] = 34821223, (u(), h)[e + 4 >> 2] = 2310721022, (u(), h)[0] = 1668509029;
        }
        function Te() {
            if (!de) {
                var e = br();
                e == 0 && (e += 4);
                var r = (u(), h)[e >> 2], t = (u(), h)[e + 4 >> 2];
                (r != 34821223 || t != 2310721022) && A(`Stack overflow! Stack cookie has been overwritten at ${ae(e)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${ae(t)} ${ae(r)}`), (u(), h)[0] != 1668509029 && A("Runtime error: The application has corrupted its heap memory area (address zero)!");
            }
        }
        function er(...e) {
            if (R) {
                let n = function(o) {
                    switch(typeof o){
                        case "object":
                            return t.inspect(o);
                        case "undefined":
                            return "undefined";
                    }
                    return o;
                };
                var r = K("fs"), t = K("util");
                r.writeSync(2, e.map(n).join(" ") + `
`);
            } else console.warn(...e);
        }
        (()=>{
            var e = new Int16Array(1), r = new Int8Array(e.buffer);
            e[0] = 25459, (r[0] !== 115 || r[1] !== 99) && A("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
        })();
        function We(e) {
            Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, {
                configurable: !0,
                set () {
                    A(`Attempt to set \`Module.${e}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
                }
            });
        }
        function F(e) {
            return ()=>c(!1, `call to '${e}' via reference taken before Wasm module initialization`);
        }
        function sn(e) {
            Object.getOwnPropertyDescriptor(f, e) && A(`\`Module.${e}\` was supplied but \`${e}\` not included in INCOMING_MODULE_JS_API`);
        }
        function ln(e) {
            return e === "FS_createPath" || e === "FS_createDataFile" || e === "FS_createPreloadedFile" || e === "FS_preloadFile" || e === "FS_unlink" || e === "addRunDependency" || e === "FS_createLazyFile" || e === "FS_createDevice" || e === "removeRunDependency";
        }
        function dn(e) {
            Ir(e);
        }
        function Ir(e) {
            y || Object.getOwnPropertyDescriptor(f, e) || Object.defineProperty(f, e, {
                configurable: !0,
                get () {
                    var r = `'${e}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
                    ln(e) && (r += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), A(r);
                }
            });
        }
        function cn() {
            function e() {
                var t = 0;
                return ue && typeof ye < "u" && (t = ye()), `w:${Dr},t:${ae(t)}:`;
            }
            var r = er;
            er = (...t)=>r(e(), ...t);
        }
        cn();
        function u() {
            z.buffer != O.buffer && xe();
        }
        var rr, tr;
        if (R && y) {
            var Mr = De.parentPort;
            Mr.on("message", (e)=>global.onmessage?.({
                    data: e
                })), Object.assign(globalThis, {
                self: global,
                postMessage: (e)=>Mr.postMessage(e)
            }), process.on("uncaughtException", (e)=>{
                postMessage({
                    cmd: "uncaughtException",
                    error: e
                }), process.exit(1);
            });
        }
        var Dr = 0, Rr;
        if (y) {
            var nr = !1;
            self.onunhandledrejection = (r)=>{
                throw r.reason || r;
            };
            async function e(r) {
                try {
                    var t = r.data, n = t.cmd;
                    if (n === "load") {
                        Dr = t.workerID;
                        let o = [];
                        self.onmessage = (a)=>o.push(a), Rr = ()=>{
                            postMessage({
                                cmd: "loaded"
                            });
                            for (let a of o)e(a);
                            self.onmessage = e;
                        };
                        for (const a of t.handlers)(!f[a] || f[a].proxy) && (f[a] = (...s)=>{
                            postMessage({
                                cmd: "callHandler",
                                handler: a,
                                args: s
                            });
                        }, a == "print" && (Q = f[a]), a == "printErr" && (b = f[a]));
                        z = t.wasmMemory, xe(), ne = t.wasmModule, Lr(), Xe();
                    } else if (n === "run") {
                        c(t.pthread_ptr), wn(t.pthread_ptr), wr(t.pthread_ptr, 0, 0, 1, 0, 0), w.threadInitTLS(), hr(t.pthread_ptr), nr || (zt(), nr = !0);
                        try {
                            await Zr(t.start_routine, t.arg);
                        } catch (o) {
                            if (o != "unwind") throw o;
                        }
                    } else t.target === "setimmediate" || (n === "checkMailbox" ? nr && je() : n && (b(`worker: received unknown command ${n}`), b(t)));
                } catch (o) {
                    throw b(`worker: onmessage() captured an uncaught exception: ${o}`), o?.stack && b(o.stack), Gt(), o;
                }
            }
            self.onmessage = e;
        }
        var O, U, ie, Fe, C, h, Nr, Ae, M, Or, ue = !1;
        function xe() {
            var e = z.buffer;
            O = new Int8Array(e), ie = new Int16Array(e), f.HEAPU8 = U = new Uint8Array(e), Fe = new Uint16Array(e), C = new Int32Array(e), h = new Uint32Array(e), Nr = new Float32Array(e), Ae = new Float64Array(e), M = new BigInt64Array(e), Or = new BigUint64Array(e);
        }
        function un() {
            if (!y) {
                if (f.wasmMemory) z = f.wasmMemory;
                else {
                    var e = f.INITIAL_MEMORY || 16777216;
                    c(e >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + e + "! (STACK_SIZE=65536)"), z = new WebAssembly.Memory({
                        initial: e / 65536,
                        maximum: 32768,
                        shared: !0
                    });
                }
                xe();
            }
        }
        c(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
        function fn() {
            if (c(!y), f.preRun) for(typeof f.preRun == "function" && (f.preRun = [
                f.preRun
            ]); f.preRun.length;)Hr(f.preRun.shift());
            We("preRun"), jr(zr);
        }
        function Wr() {
            if (c(!ue), ue = !0, y) return Rr();
            Te(), !f.noFSInit && !i.initialized && i.init(), Z.__wasm_call_ctors(), i.ignorePermissions = !1;
        }
        function _n() {
            if (Te(), !y) {
                if (f.postRun) for(typeof f.postRun == "function" && (f.postRun = [
                    f.postRun
                ]); f.postRun.length;)En(f.postRun.shift());
                We("postRun"), jr(Yr);
            }
        }
        function A(e) {
            f.onAbort?.(e), e = "Aborted(" + e + ")", b(e), de = !0;
            var r = new WebAssembly.RuntimeError(e);
            throw tr?.(r), r;
        }
        function D(e, r) {
            return (...t)=>{
                c(ue, `native function \`${e}\` called before runtime initialization`);
                var n = Z[e];
                return c(n, `exported native function \`${e}\` not found`), c(t.length <= r, `native function \`${e}\` called with ${t.length} args but expects ${r}`), n(...t);
            };
        }
        var ir;
        function mn() {
            return f.locateFile ? on("cp_sat_runtime.wasm") : new URL("/assets/cp_sat_runtime-C04Aywpi.wasm", import.meta.url).href;
        }
        function vn(e) {
            if (e == ir && ke) return new Uint8Array(ke);
            if (be) return be(e);
            throw "both async and sync fetching of the wasm failed";
        }
        async function hn(e) {
            if (!ke) try {
                var r = await Ne(e);
                return new Uint8Array(r);
            } catch  {}
            return vn(e);
        }
        async function pn(e, r) {
            try {
                var t = await hn(e), n = await WebAssembly.instantiate(t, r);
                return n;
            } catch (o) {
                b(`failed to asynchronously prepare wasm: ${o}`), Oe(e) && b(`warning: Loading from a file URI (${e}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), A(o);
            }
        }
        async function gn(e, r, t) {
            if (!e && !R) try {
                var n = fetch(r, {
                    credentials: "same-origin"
                }), o = await WebAssembly.instantiateStreaming(n, t);
                return o;
            } catch (a) {
                b(`wasm streaming compile failed: ${a}`), b("falling back to ArrayBuffer instantiation");
            }
            return pn(r, t);
        }
        function xr() {
            ao(), Ee.__instrumented || (Ee.__instrumented = !0, V.instrumentWasmImports(Ee));
            var e = {
                env: Ee,
                wasi_snapshot_preview1: Ee
            };
            return e;
        }
        async function Lr() {
            function e(l, d) {
                return Z = l.exports, Z = V.instrumentWasmExports(Z), bn(Z._emscripten_tls_init), oo(Z), ne = d, Z;
            }
            var r = f;
            function t(l) {
                return c(f === r, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), r = null, e(l.instance, l.module);
            }
            var n = xr();
            if (f.instantiateWasm) return new Promise((l, d)=>{
                try {
                    f.instantiateWasm(n, (_, v)=>{
                        l(e(_, v));
                    });
                } catch (_) {
                    b(`Module.instantiateWasm callback failed with error: ${_}`), d(_);
                }
            });
            if (y) {
                c(ne, "wasmModule should have been received via postMessage");
                var o = new WebAssembly.Instance(ne, xr());
                return e(o, ne);
            }
            ir ??= mn();
            var a = await gn(ke, ir, n), s = t(a);
            return s;
        }
        class Ur {
            name = "ExitStatus";
            constructor(r){
                this.message = `Program terminated with exit(${r})`, this.status = r;
            }
        }
        var $r = (e)=>{
            e.terminate(), e.onmessage = (r)=>{
                var t = r.data.cmd;
                b(`received "${t}" command from terminated worker: ${e.workerID}`);
            };
        }, Br = (e)=>{
            c(!y, "Internal Error! cleanupThread() can only ever be called from main application thread!"), c(e, "Internal Error! Null pthread_ptr in cleanupThread!");
            var r = w.pthreads[e];
            c(r), w.returnWorkerToPool(r);
        }, jr = (e)=>{
            for(; e.length > 0;)e.shift()(f);
        }, zr = [], Hr = (e)=>zr.push(e), oe = 0, Pe = null, fe = {}, ee = null, Vr = (e)=>{
            if (oe--, f.monitorRunDependencies?.(oe), c(e, "removeRunDependency requires an ID"), c(fe[e]), delete fe[e], oe == 0 && (ee !== null && (clearInterval(ee), ee = null), Pe)) {
                var r = Pe;
                Pe = null, r();
            }
        }, Gr = (e)=>{
            oe++, f.monitorRunDependencies?.(oe), c(e, "addRunDependency requires an ID"), c(!fe[e]), fe[e] = 1, ee === null && globalThis.setInterval && (ee = setInterval(()=>{
                if (de) {
                    clearInterval(ee), ee = null;
                    return;
                }
                var r = !1;
                for(var t in fe)r || (r = !0, b("still waiting on run dependencies:")), b(`dependency: ${t}`);
                r && b("(end of list)");
            }, 1e4), ee.unref?.());
        }, Kr = (e)=>{
            c(!y, "Internal Error! spawnThread() can only ever be called from main application thread!"), c(e.pthread_ptr, "Internal error, no pthread ptr!");
            var r = w.getNewWorker();
            if (!r) return 6;
            c(!r.pthread_ptr, "Internal error!"), w.runningWorkers.push(r), w.pthreads[e.pthread_ptr] = r, r.pthread_ptr = e.pthread_ptr;
            var t = {
                cmd: "run",
                start_routine: e.startRoutine,
                arg: e.arg,
                pthread_ptr: e.pthread_ptr
            };
            return R && r.unref(), r.postMessage(t, e.transferList), 0;
        }, _e = 0, Le = ()=>cr || _e > 0, Xr = ()=>kr(), or = (e)=>Zt(e), ar = (e)=>Qt(e), N = (e, r, t, ...n)=>{
            for(var o = n.length * 2, a = Xr(), s = ar(o * 8), l = s >> 3, d = 0; d < n.length; d++){
                var _ = n[d];
                typeof _ == "bigint" ? ((u(), M)[l + 2 * d] = 1n, (u(), M)[l + 2 * d + 1] = _) : ((u(), M)[l + 2 * d] = 0n, (u(), Ae)[l + 2 * d + 1] = _);
            }
            var v = Kt(e, r, o, s, t);
            return or(a), v;
        };
        function sr(e) {
            if (y) return N(0, 0, 1, e);
            ce = e, Le() || (w.terminateAllThreads(), f.onExit?.(e), de = !0), Ze(e, new Ur(e));
        }
        function qr(e) {
            if (y) return N(1, 0, 0, e);
            lr(e);
        }
        var yn = (e, r)=>{
            if (ce = e, lo(), y) throw c(!r), qr(e), "unwind";
            if (Le() && !r) {
                var t = `program exited (with status: ${e}), but keepRuntimeAlive() is set (counter=${_e}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
                tr?.(t), b(t);
            }
            sr(e);
        }, lr = yn, ae = (e)=>(c(typeof e == "number", `ptrToString expects a number, got ${typeof e}`), e >>>= 0, "0x" + e.toString(16).padStart(8, "0")), w = {
            unusedWorkers: [],
            runningWorkers: [],
            tlsInitFunctions: [],
            pthreads: {},
            nextWorkerID: 1,
            init () {
                y || w.initMainThread();
            },
            initMainThread () {
                for(var e = navigator.hardwareConcurrency; e--;)w.allocateUnusedWorker();
                Hr(async ()=>{
                    var r = w.loadWasmModuleToAllWorkers();
                    Gr("loading-workers"), await r, Vr("loading-workers");
                });
            },
            terminateAllThreads: ()=>{
                c(!y, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
                for (var e of w.runningWorkers)$r(e);
                for (var e of w.unusedWorkers)$r(e);
                w.unusedWorkers = [], w.runningWorkers = [], w.pthreads = {};
            },
            returnWorkerToPool: (e)=>{
                var r = e.pthread_ptr;
                delete w.pthreads[r], w.unusedWorkers.push(e), w.runningWorkers.splice(w.runningWorkers.indexOf(e), 1), e.pthread_ptr = 0, Xt(r);
            },
            threadInitTLS () {
                w.tlsInitFunctions.forEach((e)=>e());
            },
            loadWasmModuleToWorker: (e)=>new Promise((r)=>{
                    e.onmessage = (a)=>{
                        var s = a.data, l = s.cmd;
                        if (s.targetThread && s.targetThread != ye()) {
                            var d = w.pthreads[s.targetThread];
                            d ? d.postMessage(s, s.transferList) : b(`Internal error! Worker sent a message "${l}" to target pthread ${s.targetThread}, but that thread no longer exists!`);
                            return;
                        }
                        l === "checkMailbox" ? je() : l === "spawnThread" ? Kr(s) : l === "cleanupThread" ? Tt(()=>Br(s.thread)) : l === "loaded" ? (e.loaded = !0, R && !e.pthread_ptr && e.unref(), r(e)) : s.target === "setimmediate" ? e.postMessage(s) : l === "uncaughtException" ? e.onerror(s.error) : l === "callHandler" ? f[s.handler](...s.args) : l && b(`worker sent an unknown command ${l}`);
                    }, e.onerror = (a)=>{
                        var s = "worker sent an error!";
                        throw e.pthread_ptr && (s = `Pthread ${ae(e.pthread_ptr)} sent an error!`), b(`${s} ${a.filename}:${a.lineno}: ${a.message}`), a;
                    }, R && (e.on("message", (a)=>e.onmessage({
                            data: a
                        })), e.on("error", (a)=>e.onerror(a))), c(z instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), c(ne instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
                    var t = [], n = [
                        "onExit",
                        "onAbort",
                        "print",
                        "printErr"
                    ];
                    for (var o of n)f.propertyIsEnumerable(o) && t.push(o);
                    e.postMessage({
                        cmd: "load",
                        handlers: t,
                        wasmMemory: z,
                        wasmModule: ne,
                        workerID: e.workerID
                    });
                }),
            async loadWasmModuleToAllWorkers () {
                return y ? void 0 : Promise.all(w.unusedWorkers.map(w.loadWasmModuleToWorker));
            },
            allocateUnusedWorker () {
                var e;
                if (f.mainScriptUrlOrBlob) {
                    var r = f.mainScriptUrlOrBlob;
                    typeof r != "string" && (r = URL.createObjectURL(r)), e = new Worker(r, {
                        type: "module",
                        workerData: "em-pthread",
                        name: "em-pthread-" + w.nextWorkerID
                    });
                } else e = new Worker(self.location.href, {
                    type: "module",
                    workerData: "em-pthread",
                    name: "em-pthread-" + w.nextWorkerID
                });
                e.workerID = w.nextWorkerID++, w.unusedWorkers.push(e);
            },
            getNewWorker () {
                return w.unusedWorkers.length == 0 && (R || b("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.\nIf you want to throw an explicit error instead of the risk of deadlocking in those cases, use setting `-sPTHREAD_POOL_SIZE_STRICT=2`."), w.allocateUnusedWorker(), w.loadWasmModuleToWorker(w.unusedWorkers[0])), w.unusedWorkers.pop();
            }
        }, Yr = [], En = (e)=>Yr.push(e);
        function wn(e) {
            var r = (u(), h)[e + 52 >> 2], t = (u(), h)[e + 56 >> 2], n = r - t;
            c(r != 0), c(n != 0), c(r > n, "stackHigh must be higher then stackLow"), Jt(r, n), or(r), Cr();
        }
        var dr = [], Jr = (e)=>{
            var r = dr[e];
            return r || (dr[e] = r = en.get(e), V.isAsyncExport(r) && (dr[e] = r = V.makeAsyncFunction(r))), r;
        }, Zr = async (e, r)=>{
            _e = 0, cr = 0;
            var t = WebAssembly.promising(Jr(e))(r);
            Te();
            function n(o) {
                if (Le()) {
                    ce = o;
                    return;
                }
                Sr(o);
            }
            t = await t, n(t);
        };
        Zr.isAsync = !0;
        var cr = !0, bn = (e)=>w.tlsInitFunctions.push(e), re = (e)=>{
            re.shown ||= {}, re.shown[e] || (re.shown[e] = 1, R && (e = "warning: " + e), b(e));
        }, z, Qr = globalThis.TextDecoder && new TextDecoder(), et = (e, r, t, n)=>{
            var o = r + t;
            if (n) return o;
            for(; e[r] && !(r >= o);)++r;
            return r;
        }, me = (e, r = 0, t, n)=>{
            var o = et(e, r, t, n);
            if (o - r > 16 && e.buffer && Qr) return Qr.decode(e.buffer instanceof ArrayBuffer ? e.subarray(r, o) : e.slice(r, o));
            for(var a = ""; r < o;){
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
                if ((s & 240) == 224 ? s = (s & 15) << 12 | l << 6 | d : ((s & 248) != 240 && re("Invalid UTF-8 leading byte " + ae(s) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), s = (s & 7) << 18 | l << 12 | d << 6 | e[r++] & 63), s < 65536) a += String.fromCharCode(s);
                else {
                    var _ = s - 65536;
                    a += String.fromCharCode(55296 | _ >> 10, 56320 | _ & 1023);
                }
            }
            return a;
        }, X = (e, r, t)=>(c(typeof e == "number", `UTF8ToString expects a number (got ${typeof e})`), e ? me((u(), U), e, r, t) : ""), Sn = (e, r, t, n)=>A(`Assertion failed: ${X(e)}, at: ` + [
                r ? X(r) : "unknown filename",
                t,
                n ? X(n) : "unknown function"
            ]);
        class kn {
            constructor(r){
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
        var Tn = (e, r, t)=>{
            var n = new kn(e);
            n.init(r, t), c(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
        };
        function rt(e, r, t, n) {
            return y ? N(2, 0, 1, e, r, t, n) : tt(e, r, t, n);
        }
        var Fn = ()=>!!globalThis.SharedArrayBuffer, tt = (e, r, t, n)=>{
            if (!Fn()) return er("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
            var o = [], a = 0;
            if (y && (o.length === 0 || a)) return rt(e, r, t, n);
            var s = {
                startRoutine: t,
                pthread_ptr: e,
                arg: n,
                transferList: o
            };
            return y ? (s.cmd = "spawnThread", postMessage(s, o), 0) : Kr(s);
        }, Ue = ()=>{
            c(P.varargs != null);
            var e = (u(), C)[+P.varargs >> 2];
            return P.varargs += 4, e;
        }, ve = Ue, T = {
            isAbs: (e)=>e.charAt(0) === "/",
            splitPath: (e)=>{
                var r = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
                return r.exec(e).slice(1);
            },
            normalizeArray: (e, r)=>{
                for(var t = 0, n = e.length - 1; n >= 0; n--){
                    var o = e[n];
                    o === "." ? e.splice(n, 1) : o === ".." ? (e.splice(n, 1), t++) : t && (e.splice(n, 1), t--);
                }
                if (r) for(; t; t--)e.unshift("..");
                return e;
            },
            normalize: (e)=>{
                var r = T.isAbs(e), t = e.slice(-1) === "/";
                return e = T.normalizeArray(e.split("/").filter((n)=>!!n), !r).join("/"), !e && !r && (e = "."), e && t && (e += "/"), (r ? "/" : "") + e;
            },
            dirname: (e)=>{
                var r = T.splitPath(e), t = r[0], n = r[1];
                return !t && !n ? "." : (n && (n = n.slice(0, -1)), t + n);
            },
            basename: (e)=>e && e.match(/([^\/]+|\/)\/*$/)[1],
            join: (...e)=>T.normalize(e.join("/")),
            join2: (e, r)=>T.normalize(e + "/" + r)
        }, An = ()=>{
            if (R) {
                var e = K("crypto");
                return (r)=>e.randomFillSync(r);
            }
            return (r)=>r.set(crypto.getRandomValues(new Uint8Array(r.byteLength)));
        }, ur = (e)=>{
            (ur = An())(e);
        }, he = {
            resolve: (...e)=>{
                for(var r = "", t = !1, n = e.length - 1; n >= -1 && !t; n--){
                    var o = n >= 0 ? e[n] : i.cwd();
                    if (typeof o != "string") throw new TypeError("Arguments to path.resolve must be strings");
                    if (!o) return "";
                    r = o + "/" + r, t = T.isAbs(o);
                }
                return r = T.normalizeArray(r.split("/").filter((a)=>!!a), !t).join("/"), (t ? "/" : "") + r || ".";
            },
            relative: (e, r)=>{
                e = he.resolve(e).slice(1), r = he.resolve(r).slice(1);
                function t(_) {
                    for(var v = 0; v < _.length && _[v] === ""; v++);
                    for(var g = _.length - 1; g >= 0 && _[g] === ""; g--);
                    return v > g ? [] : _.slice(v, g - v + 1);
                }
                for(var n = t(e.split("/")), o = t(r.split("/")), a = Math.min(n.length, o.length), s = a, l = 0; l < a; l++)if (n[l] !== o[l]) {
                    s = l;
                    break;
                }
                for(var d = [], l = s; l < n.length; l++)d.push("..");
                return d = d.concat(o.slice(s)), d.join("/");
            }
        }, fr = [], se = (e)=>{
            for(var r = 0, t = 0; t < e.length; ++t){
                var n = e.charCodeAt(t);
                n <= 127 ? r++ : n <= 2047 ? r += 2 : n >= 55296 && n <= 57343 ? (r += 4, ++t) : r += 3;
            }
            return r;
        }, nt = (e, r, t, n)=>{
            if (c(typeof e == "string", `stringToUTF8Array expects a string (got ${typeof e})`), !(n > 0)) return 0;
            for(var o = t, a = t + n - 1, s = 0; s < e.length; ++s){
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
                    l > 1114111 && re("Invalid Unicode code point " + ae(l) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), r[t++] = 240 | l >> 18, r[t++] = 128 | l >> 12 & 63, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63, s++;
                }
            }
            return r[t] = 0, t - o;
        }, _r = (e, r, t)=>{
            var n = se(e) + 1, o = new Array(n), a = nt(e, o, 0, o.length);
            return o.length = a, o;
        }, Pn = ()=>{
            if (!fr.length) {
                var e = null;
                if (R) {
                    var r = 256, t = Buffer.alloc(r), n = 0, o = process.stdin.fd;
                    try {
                        n = Se.readSync(o, t, 0, r);
                    } catch (a) {
                        if (a.toString().includes("EOF")) n = 0;
                        else throw a;
                    }
                    n > 0 && (e = t.slice(0, n).toString("utf-8"));
                } else globalThis.window?.prompt && (e = window.prompt("Input: "), e !== null && (e += `
`));
                if (!e) return null;
                fr = _r(e);
            }
            return fr.shift();
        }, te = {
            ttys: [],
            init () {},
            shutdown () {},
            register (e, r) {
                te.ttys[e] = {
                    input: [],
                    output: [],
                    ops: r
                }, i.registerDevice(e, te.stream_ops);
            },
            stream_ops: {
                open (e) {
                    var r = te.ttys[e.node.rdev];
                    if (!r) throw new i.ErrnoError(43);
                    e.tty = r, e.seekable = !1;
                },
                close (e) {
                    e.tty.ops.fsync(e.tty);
                },
                fsync (e) {
                    e.tty.ops.fsync(e.tty);
                },
                read (e, r, t, n, o) {
                    if (!e.tty || !e.tty.ops.get_char) throw new i.ErrnoError(60);
                    for(var a = 0, s = 0; s < n; s++){
                        var l;
                        try {
                            l = e.tty.ops.get_char(e.tty);
                        } catch  {
                            throw new i.ErrnoError(29);
                        }
                        if (l === void 0 && a === 0) throw new i.ErrnoError(6);
                        if (l == null) break;
                        a++, r[t + s] = l;
                    }
                    return a && (e.node.atime = Date.now()), a;
                },
                write (e, r, t, n, o) {
                    if (!e.tty || !e.tty.ops.put_char) throw new i.ErrnoError(60);
                    try {
                        for(var a = 0; a < n; a++)e.tty.ops.put_char(e.tty, r[t + a]);
                    } catch  {
                        throw new i.ErrnoError(29);
                    }
                    return n && (e.node.mtime = e.node.ctime = Date.now()), a;
                }
            },
            default_tty_ops: {
                get_char (e) {
                    return Pn();
                },
                put_char (e, r) {
                    r === null || r === 10 ? (Q(me(e.output)), e.output = []) : r != 0 && e.output.push(r);
                },
                fsync (e) {
                    e.output?.length > 0 && (Q(me(e.output)), e.output = []);
                },
                ioctl_tcgets (e) {
                    return {
                        c_iflag: 25856,
                        c_oflag: 5,
                        c_cflag: 191,
                        c_lflag: 35387,
                        c_cc: [
                            3,
                            28,
                            127,
                            21,
                            4,
                            0,
                            1,
                            0,
                            17,
                            19,
                            26,
                            0,
                            18,
                            15,
                            23,
                            22,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0
                        ]
                    };
                },
                ioctl_tcsets (e, r, t) {
                    return 0;
                },
                ioctl_tiocgwinsz (e) {
                    return [
                        24,
                        80
                    ];
                }
            },
            default_tty1_ops: {
                put_char (e, r) {
                    r === null || r === 10 ? (b(me(e.output)), e.output = []) : r != 0 && e.output.push(r);
                },
                fsync (e) {
                    e.output?.length > 0 && (b(me(e.output)), e.output = []);
                }
            }
        }, Cn = (e, r)=>(u(), U).fill(0, e, e + r), it = (e, r)=>(c(r, "alignment argument is required"), Math.ceil(e / r) * r), ot = (e)=>{
            e = it(e, 65536);
            var r = Vt(65536, e);
            return r && Cn(r, e), r;
        }, E = {
            ops_table: null,
            mount (e) {
                return E.createNode(null, "/", 16895, 0);
            },
            createNode (e, r, t, n) {
                if (i.isBlkdev(t) || i.isFIFO(t)) throw new i.ErrnoError(63);
                E.ops_table ||= {
                    dir: {
                        node: {
                            getattr: E.node_ops.getattr,
                            setattr: E.node_ops.setattr,
                            lookup: E.node_ops.lookup,
                            mknod: E.node_ops.mknod,
                            rename: E.node_ops.rename,
                            unlink: E.node_ops.unlink,
                            rmdir: E.node_ops.rmdir,
                            readdir: E.node_ops.readdir,
                            symlink: E.node_ops.symlink
                        },
                        stream: {
                            llseek: E.stream_ops.llseek
                        }
                    },
                    file: {
                        node: {
                            getattr: E.node_ops.getattr,
                            setattr: E.node_ops.setattr
                        },
                        stream: {
                            llseek: E.stream_ops.llseek,
                            read: E.stream_ops.read,
                            write: E.stream_ops.write,
                            mmap: E.stream_ops.mmap,
                            msync: E.stream_ops.msync
                        }
                    },
                    link: {
                        node: {
                            getattr: E.node_ops.getattr,
                            setattr: E.node_ops.setattr,
                            readlink: E.node_ops.readlink
                        },
                        stream: {}
                    },
                    chrdev: {
                        node: {
                            getattr: E.node_ops.getattr,
                            setattr: E.node_ops.setattr
                        },
                        stream: i.chrdev_stream_ops
                    }
                };
                var o = i.createNode(e, r, t, n);
                return i.isDir(o.mode) ? (o.node_ops = E.ops_table.dir.node, o.stream_ops = E.ops_table.dir.stream, o.contents = {}) : i.isFile(o.mode) ? (o.node_ops = E.ops_table.file.node, o.stream_ops = E.ops_table.file.stream, o.usedBytes = 0, o.contents = null) : i.isLink(o.mode) ? (o.node_ops = E.ops_table.link.node, o.stream_ops = E.ops_table.link.stream) : i.isChrdev(o.mode) && (o.node_ops = E.ops_table.chrdev.node, o.stream_ops = E.ops_table.chrdev.stream), o.atime = o.mtime = o.ctime = Date.now(), e && (e.contents[r] = o, e.atime = e.mtime = e.ctime = o.atime), o;
            },
            getFileDataAsTypedArray (e) {
                return e.contents ? e.contents.subarray ? e.contents.subarray(0, e.usedBytes) : new Uint8Array(e.contents) : new Uint8Array(0);
            },
            expandFileStorage (e, r) {
                var t = e.contents ? e.contents.length : 0;
                if (!(t >= r)) {
                    var n = 1024 * 1024;
                    r = Math.max(r, t * (t < n ? 2 : 1.125) >>> 0), t != 0 && (r = Math.max(r, 256));
                    var o = e.contents;
                    e.contents = new Uint8Array(r), e.usedBytes > 0 && e.contents.set(o.subarray(0, e.usedBytes), 0);
                }
            },
            resizeFileStorage (e, r) {
                if (e.usedBytes != r) if (r == 0) e.contents = null, e.usedBytes = 0;
                else {
                    var t = e.contents;
                    e.contents = new Uint8Array(r), t && e.contents.set(t.subarray(0, Math.min(r, e.usedBytes))), e.usedBytes = r;
                }
            },
            node_ops: {
                getattr (e) {
                    var r = {};
                    return r.dev = i.isChrdev(e.mode) ? e.id : 1, r.ino = e.id, r.mode = e.mode, r.nlink = 1, r.uid = 0, r.gid = 0, r.rdev = e.rdev, i.isDir(e.mode) ? r.size = 4096 : i.isFile(e.mode) ? r.size = e.usedBytes : i.isLink(e.mode) ? r.size = e.link.length : r.size = 0, r.atime = new Date(e.atime), r.mtime = new Date(e.mtime), r.ctime = new Date(e.ctime), r.blksize = 4096, r.blocks = Math.ceil(r.size / r.blksize), r;
                },
                setattr (e, r) {
                    for (const t of [
                        "mode",
                        "atime",
                        "mtime",
                        "ctime"
                    ])r[t] != null && (e[t] = r[t]);
                    r.size !== void 0 && E.resizeFileStorage(e, r.size);
                },
                lookup (e, r) {
                    throw new i.ErrnoError(44);
                },
                mknod (e, r, t, n) {
                    return E.createNode(e, r, t, n);
                },
                rename (e, r, t) {
                    var n;
                    try {
                        n = i.lookupNode(r, t);
                    } catch  {}
                    if (n) {
                        if (i.isDir(e.mode)) for(var o in n.contents)throw new i.ErrnoError(55);
                        i.hashRemoveNode(n);
                    }
                    delete e.parent.contents[e.name], r.contents[t] = e, e.name = t, r.ctime = r.mtime = e.parent.ctime = e.parent.mtime = Date.now();
                },
                unlink (e, r) {
                    delete e.contents[r], e.ctime = e.mtime = Date.now();
                },
                rmdir (e, r) {
                    var t = i.lookupNode(e, r);
                    for(var n in t.contents)throw new i.ErrnoError(55);
                    delete e.contents[r], e.ctime = e.mtime = Date.now();
                },
                readdir (e) {
                    return [
                        ".",
                        "..",
                        ...Object.keys(e.contents)
                    ];
                },
                symlink (e, r, t) {
                    var n = E.createNode(e, r, 41471, 0);
                    return n.link = t, n;
                },
                readlink (e) {
                    if (!i.isLink(e.mode)) throw new i.ErrnoError(28);
                    return e.link;
                }
            },
            stream_ops: {
                read (e, r, t, n, o) {
                    var a = e.node.contents;
                    if (o >= e.node.usedBytes) return 0;
                    var s = Math.min(e.node.usedBytes - o, n);
                    if (c(s >= 0), s > 8 && a.subarray) r.set(a.subarray(o, o + s), t);
                    else for(var l = 0; l < s; l++)r[t + l] = a[o + l];
                    return s;
                },
                write (e, r, t, n, o, a) {
                    if (c(!(r instanceof ArrayBuffer)), r.buffer === (u(), O).buffer && (a = !1), !n) return 0;
                    var s = e.node;
                    if (s.mtime = s.ctime = Date.now(), r.subarray && (!s.contents || s.contents.subarray)) {
                        if (a) return c(o === 0, "canOwn must imply no weird position inside the file"), s.contents = r.subarray(t, t + n), s.usedBytes = n, n;
                        if (s.usedBytes === 0 && o === 0) return s.contents = r.slice(t, t + n), s.usedBytes = n, n;
                        if (o + n <= s.usedBytes) return s.contents.set(r.subarray(t, t + n), o), n;
                    }
                    if (E.expandFileStorage(s, o + n), s.contents.subarray && r.subarray) s.contents.set(r.subarray(t, t + n), o);
                    else for(var l = 0; l < n; l++)s.contents[o + l] = r[t + l];
                    return s.usedBytes = Math.max(s.usedBytes, o + n), n;
                },
                llseek (e, r, t) {
                    var n = r;
                    if (t === 1 ? n += e.position : t === 2 && i.isFile(e.node.mode) && (n += e.node.usedBytes), n < 0) throw new i.ErrnoError(28);
                    return n;
                },
                mmap (e, r, t, n, o) {
                    if (!i.isFile(e.node.mode)) throw new i.ErrnoError(43);
                    var a, s, l = e.node.contents;
                    if (!(o & 2) && l && l.buffer === (u(), O).buffer) s = !1, a = l.byteOffset;
                    else {
                        if (s = !0, a = ot(r), !a) throw new i.ErrnoError(48);
                        l && ((t > 0 || t + r < l.length) && (l.subarray ? l = l.subarray(t, t + r) : l = Array.prototype.slice.call(l, t, t + r)), (u(), O).set(l, a));
                    }
                    return {
                        ptr: a,
                        allocated: s
                    };
                },
                msync (e, r, t, n, o) {
                    return E.stream_ops.write(e, r, 0, n, t, !1), 0;
                }
            }
        }, In = (e)=>{
            var r = {
                r: 0,
                "r+": 2,
                w: 577,
                "w+": 578,
                a: 1089,
                "a+": 1090
            }, t = r[e];
            if (typeof t > "u") throw new Error(`Unknown file open mode: ${e}`);
            return t;
        }, mr = (e, r)=>{
            var t = 0;
            return e && (t |= 365), r && (t |= 146), t;
        }, Mn = (e)=>X(Ht(e)), at = {
            EPERM: 63,
            ENOENT: 44,
            ESRCH: 71,
            EINTR: 27,
            EIO: 29,
            ENXIO: 60,
            E2BIG: 1,
            ENOEXEC: 45,
            EBADF: 8,
            ECHILD: 12,
            EAGAIN: 6,
            EWOULDBLOCK: 6,
            ENOMEM: 48,
            EACCES: 2,
            EFAULT: 21,
            ENOTBLK: 105,
            EBUSY: 10,
            EEXIST: 20,
            EXDEV: 75,
            ENODEV: 43,
            ENOTDIR: 54,
            EISDIR: 31,
            EINVAL: 28,
            ENFILE: 41,
            EMFILE: 33,
            ENOTTY: 59,
            ETXTBSY: 74,
            EFBIG: 22,
            ENOSPC: 51,
            ESPIPE: 70,
            EROFS: 69,
            EMLINK: 34,
            EPIPE: 64,
            EDOM: 18,
            ERANGE: 68,
            ENOMSG: 49,
            EIDRM: 24,
            ECHRNG: 106,
            EL2NSYNC: 156,
            EL3HLT: 107,
            EL3RST: 108,
            ELNRNG: 109,
            EUNATCH: 110,
            ENOCSI: 111,
            EL2HLT: 112,
            EDEADLK: 16,
            ENOLCK: 46,
            EBADE: 113,
            EBADR: 114,
            EXFULL: 115,
            ENOANO: 104,
            EBADRQC: 103,
            EBADSLT: 102,
            EDEADLOCK: 16,
            EBFONT: 101,
            ENOSTR: 100,
            ENODATA: 116,
            ETIME: 117,
            ENOSR: 118,
            ENONET: 119,
            ENOPKG: 120,
            EREMOTE: 121,
            ENOLINK: 47,
            EADV: 122,
            ESRMNT: 123,
            ECOMM: 124,
            EPROTO: 65,
            EMULTIHOP: 36,
            EDOTDOT: 125,
            EBADMSG: 9,
            ENOTUNIQ: 126,
            EBADFD: 127,
            EREMCHG: 128,
            ELIBACC: 129,
            ELIBBAD: 130,
            ELIBSCN: 131,
            ELIBMAX: 132,
            ELIBEXEC: 133,
            ENOSYS: 52,
            ENOTEMPTY: 55,
            ENAMETOOLONG: 37,
            ELOOP: 32,
            EOPNOTSUPP: 138,
            EPFNOSUPPORT: 139,
            ECONNRESET: 15,
            ENOBUFS: 42,
            EAFNOSUPPORT: 5,
            EPROTOTYPE: 67,
            ENOTSOCK: 57,
            ENOPROTOOPT: 50,
            ESHUTDOWN: 140,
            ECONNREFUSED: 14,
            EADDRINUSE: 3,
            ECONNABORTED: 13,
            ENETUNREACH: 40,
            ENETDOWN: 38,
            ETIMEDOUT: 73,
            EHOSTDOWN: 142,
            EHOSTUNREACH: 23,
            EINPROGRESS: 26,
            EALREADY: 7,
            EDESTADDRREQ: 17,
            EMSGSIZE: 35,
            EPROTONOSUPPORT: 66,
            ESOCKTNOSUPPORT: 137,
            EADDRNOTAVAIL: 4,
            ENETRESET: 39,
            EISCONN: 30,
            ENOTCONN: 53,
            ETOOMANYREFS: 141,
            EUSERS: 136,
            EDQUOT: 19,
            ESTALE: 72,
            ENOTSUP: 138,
            ENOMEDIUM: 148,
            EILSEQ: 25,
            EOVERFLOW: 61,
            ECANCELED: 11,
            ENOTRECOVERABLE: 56,
            EOWNERDEAD: 62,
            ESTRPIPE: 135
        }, Dn = async (e)=>{
            var r = await Ne(e);
            return c(r, `Loading data file "${e}" failed (no arrayBuffer).`), new Uint8Array(r);
        }, Rn = (...e)=>i.createDataFile(...e), Nn = (e)=>{
            for(var r = e;;){
                if (!fe[e]) return e;
                e = r + Math.random();
            }
        }, st = [], On = async (e, r)=>{
            typeof Browser < "u" && Browser.init();
            for (var t of st)if (t.canHandle(r)) return c(t.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), t.handle(e, r);
            return e;
        }, lt = async (e, r, t, n, o, a, s, l)=>{
            var d = r ? he.resolve(T.join2(e, r)) : e, _ = Nn(`cp ${d}`);
            Gr(_);
            try {
                var v = t;
                typeof t == "string" && (v = await Dn(t)), v = await On(v, d), l?.(), a || Rn(e, r, v, n, o, s);
            } finally{
                Vr(_);
            }
        }, Wn = (e, r, t, n, o, a, s, l, d, _)=>{
            lt(e, r, t, n, o, l, d, _).then(a).catch(s);
        }, i = {
            root: null,
            mounts: [],
            devices: {},
            streams: [],
            nextInode: 1,
            nameTable: null,
            currentPath: "/",
            initialized: !1,
            ignorePermissions: !0,
            filesystems: null,
            syncFSRequests: 0,
            readFiles: {},
            ErrnoError: class extends Error {
                name = "ErrnoError";
                constructor(e){
                    super(ue ? Mn(e) : ""), this.errno = e;
                    for(var r in at)if (at[r] === e) {
                        this.code = r;
                        break;
                    }
                }
            },
            FSStream: class {
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
            },
            FSNode: class {
                node_ops = {};
                stream_ops = {};
                readMode = 365;
                writeMode = 146;
                mounted = null;
                constructor(e, r, t, n){
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
            },
            lookupPath (e, r = {}) {
                if (!e) throw new i.ErrnoError(44);
                r.follow_mount ??= !0, T.isAbs(e) || (e = i.cwd() + "/" + e);
                e: for(var t = 0; t < 40; t++){
                    for(var n = e.split("/").filter((_)=>!!_), o = i.root, a = "/", s = 0; s < n.length; s++){
                        var l = s === n.length - 1;
                        if (l && r.parent) break;
                        if (n[s] !== ".") {
                            if (n[s] === "..") {
                                if (a = T.dirname(a), i.isRoot(o)) {
                                    e = a + "/" + n.slice(s + 1).join("/"), t--;
                                    continue e;
                                } else o = o.parent;
                                continue;
                            }
                            a = T.join2(a, n[s]);
                            try {
                                o = i.lookupNode(o, n[s]);
                            } catch (_) {
                                if (_?.errno === 44 && l && r.noent_okay) return {
                                    path: a
                                };
                                throw _;
                            }
                            if (i.isMountpoint(o) && (!l || r.follow_mount) && (o = o.mounted.root), i.isLink(o.mode) && (!l || r.follow)) {
                                if (!o.node_ops.readlink) throw new i.ErrnoError(52);
                                var d = o.node_ops.readlink(o);
                                T.isAbs(d) || (d = T.dirname(a) + "/" + d), e = d + "/" + n.slice(s + 1).join("/");
                                continue e;
                            }
                        }
                    }
                    return {
                        path: a,
                        node: o
                    };
                }
                throw new i.ErrnoError(32);
            },
            getPath (e) {
                for(var r;;){
                    if (i.isRoot(e)) {
                        var t = e.mount.mountpoint;
                        return r ? t[t.length - 1] !== "/" ? `${t}/${r}` : t + r : t;
                    }
                    r = r ? `${e.name}/${r}` : e.name, e = e.parent;
                }
            },
            hashName (e, r) {
                for(var t = 0, n = 0; n < r.length; n++)t = (t << 5) - t + r.charCodeAt(n) | 0;
                return (e + t >>> 0) % i.nameTable.length;
            },
            hashAddNode (e) {
                var r = i.hashName(e.parent.id, e.name);
                e.name_next = i.nameTable[r], i.nameTable[r] = e;
            },
            hashRemoveNode (e) {
                var r = i.hashName(e.parent.id, e.name);
                if (i.nameTable[r] === e) i.nameTable[r] = e.name_next;
                else for(var t = i.nameTable[r]; t;){
                    if (t.name_next === e) {
                        t.name_next = e.name_next;
                        break;
                    }
                    t = t.name_next;
                }
            },
            lookupNode (e, r) {
                var t = i.mayLookup(e);
                if (t) throw new i.ErrnoError(t);
                for(var n = i.hashName(e.id, r), o = i.nameTable[n]; o; o = o.name_next){
                    var a = o.name;
                    if (o.parent.id === e.id && a === r) return o;
                }
                return i.lookup(e, r);
            },
            createNode (e, r, t, n) {
                c(typeof e == "object");
                var o = new i.FSNode(e, r, t, n);
                return i.hashAddNode(o), o;
            },
            destroyNode (e) {
                i.hashRemoveNode(e);
            },
            isRoot (e) {
                return e === e.parent;
            },
            isMountpoint (e) {
                return !!e.mounted;
            },
            isFile (e) {
                return (e & 61440) === 32768;
            },
            isDir (e) {
                return (e & 61440) === 16384;
            },
            isLink (e) {
                return (e & 61440) === 40960;
            },
            isChrdev (e) {
                return (e & 61440) === 8192;
            },
            isBlkdev (e) {
                return (e & 61440) === 24576;
            },
            isFIFO (e) {
                return (e & 61440) === 4096;
            },
            isSocket (e) {
                return (e & 49152) === 49152;
            },
            flagsToPermissionString (e) {
                var r = [
                    "r",
                    "w",
                    "rw"
                ][e & 3];
                return e & 512 && (r += "w"), r;
            },
            nodePermissions (e, r) {
                return i.ignorePermissions ? 0 : r.includes("r") && !(e.mode & 292) || r.includes("w") && !(e.mode & 146) || r.includes("x") && !(e.mode & 73) ? 2 : 0;
            },
            mayLookup (e) {
                if (!i.isDir(e.mode)) return 54;
                var r = i.nodePermissions(e, "x");
                return r || (e.node_ops.lookup ? 0 : 2);
            },
            mayCreate (e, r) {
                if (!i.isDir(e.mode)) return 54;
                try {
                    var t = i.lookupNode(e, r);
                    return 20;
                } catch  {}
                return i.nodePermissions(e, "wx");
            },
            mayDelete (e, r, t) {
                var n;
                try {
                    n = i.lookupNode(e, r);
                } catch (a) {
                    return a.errno;
                }
                var o = i.nodePermissions(e, "wx");
                if (o) return o;
                if (t) {
                    if (!i.isDir(n.mode)) return 54;
                    if (i.isRoot(n) || i.getPath(n) === i.cwd()) return 10;
                } else if (i.isDir(n.mode)) return 31;
                return 0;
            },
            mayOpen (e, r) {
                return e ? i.isLink(e.mode) ? 32 : i.isDir(e.mode) && (i.flagsToPermissionString(r) !== "r" || r & 576) ? 31 : i.nodePermissions(e, i.flagsToPermissionString(r)) : 44;
            },
            checkOpExists (e, r) {
                if (!e) throw new i.ErrnoError(r);
                return e;
            },
            MAX_OPEN_FDS: 4096,
            nextfd () {
                for(var e = 0; e <= i.MAX_OPEN_FDS; e++)if (!i.streams[e]) return e;
                throw new i.ErrnoError(33);
            },
            getStreamChecked (e) {
                var r = i.getStream(e);
                if (!r) throw new i.ErrnoError(8);
                return r;
            },
            getStream: (e)=>i.streams[e],
            createStream (e, r = -1) {
                return c(r >= -1), e = Object.assign(new i.FSStream(), e), r == -1 && (r = i.nextfd()), e.fd = r, i.streams[r] = e, e;
            },
            closeStream (e) {
                i.streams[e] = null;
            },
            dupStream (e, r = -1) {
                var t = i.createStream(e, r);
                return t.stream_ops?.dup?.(t), t;
            },
            doSetAttr (e, r, t) {
                var n = e?.stream_ops.setattr, o = n ? e : r;
                n ??= r.node_ops.setattr, i.checkOpExists(n, 63), n(o, t);
            },
            chrdev_stream_ops: {
                open (e) {
                    var r = i.getDevice(e.node.rdev);
                    e.stream_ops = r.stream_ops, e.stream_ops.open?.(e);
                },
                llseek () {
                    throw new i.ErrnoError(70);
                }
            },
            major: (e)=>e >> 8,
            minor: (e)=>e & 255,
            makedev: (e, r)=>e << 8 | r,
            registerDevice (e, r) {
                i.devices[e] = {
                    stream_ops: r
                };
            },
            getDevice: (e)=>i.devices[e],
            getMounts (e) {
                for(var r = [], t = [
                    e
                ]; t.length;){
                    var n = t.pop();
                    r.push(n), t.push(...n.mounts);
                }
                return r;
            },
            syncfs (e, r) {
                typeof e == "function" && (r = e, e = !1), i.syncFSRequests++, i.syncFSRequests > 1 && b(`warning: ${i.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
                var t = i.getMounts(i.root.mount), n = 0;
                function o(l) {
                    return c(i.syncFSRequests > 0), i.syncFSRequests--, r(l);
                }
                function a(l) {
                    if (l) return a.errored ? void 0 : (a.errored = !0, o(l));
                    ++n >= t.length && o(null);
                }
                for (var s of t)s.type.syncfs ? s.type.syncfs(s, e, a) : a(null);
            },
            mount (e, r, t) {
                if (typeof e == "string") throw e;
                var n = t === "/", o = !t, a;
                if (n && i.root) throw new i.ErrnoError(10);
                if (!n && !o) {
                    var s = i.lookupPath(t, {
                        follow_mount: !1
                    });
                    if (t = s.path, a = s.node, i.isMountpoint(a)) throw new i.ErrnoError(10);
                    if (!i.isDir(a.mode)) throw new i.ErrnoError(54);
                }
                var l = {
                    type: e,
                    opts: r,
                    mountpoint: t,
                    mounts: []
                }, d = e.mount(l);
                return d.mount = l, l.root = d, n ? i.root = d : a && (a.mounted = l, a.mount && a.mount.mounts.push(l)), d;
            },
            unmount (e) {
                var r = i.lookupPath(e, {
                    follow_mount: !1
                });
                if (!i.isMountpoint(r.node)) throw new i.ErrnoError(28);
                var t = r.node, n = t.mounted, o = i.getMounts(n);
                for (var [a, s] of Object.entries(i.nameTable))for(; s;){
                    var l = s.name_next;
                    o.includes(s.mount) && i.destroyNode(s), s = l;
                }
                t.mounted = null;
                var d = t.mount.mounts.indexOf(n);
                c(d !== -1), t.mount.mounts.splice(d, 1);
            },
            lookup (e, r) {
                return e.node_ops.lookup(e, r);
            },
            mknod (e, r, t) {
                var n = i.lookupPath(e, {
                    parent: !0
                }), o = n.node, a = T.basename(e);
                if (!a) throw new i.ErrnoError(28);
                if (a === "." || a === "..") throw new i.ErrnoError(20);
                var s = i.mayCreate(o, a);
                if (s) throw new i.ErrnoError(s);
                if (!o.node_ops.mknod) throw new i.ErrnoError(63);
                return o.node_ops.mknod(o, a, r, t);
            },
            statfs (e) {
                return i.statfsNode(i.lookupPath(e, {
                    follow: !0
                }).node);
            },
            statfsStream (e) {
                return i.statfsNode(e.node);
            },
            statfsNode (e) {
                var r = {
                    bsize: 4096,
                    frsize: 4096,
                    blocks: 1e6,
                    bfree: 5e5,
                    bavail: 5e5,
                    files: i.nextInode,
                    ffree: i.nextInode - 1,
                    fsid: 42,
                    flags: 2,
                    namelen: 255
                };
                return e.node_ops.statfs && Object.assign(r, e.node_ops.statfs(e.mount.opts.root)), r;
            },
            create (e, r = 438) {
                return r &= 4095, r |= 32768, i.mknod(e, r, 0);
            },
            mkdir (e, r = 511) {
                return r &= 1023, r |= 16384, i.mknod(e, r, 0);
            },
            mkdirTree (e, r) {
                var t = e.split("/"), n = "";
                for (var o of t)if (o) {
                    (n || T.isAbs(e)) && (n += "/"), n += o;
                    try {
                        i.mkdir(n, r);
                    } catch (a) {
                        if (a.errno != 20) throw a;
                    }
                }
            },
            mkdev (e, r, t) {
                return typeof t > "u" && (t = r, r = 438), r |= 8192, i.mknod(e, r, t);
            },
            symlink (e, r) {
                if (!he.resolve(e)) throw new i.ErrnoError(44);
                var t = i.lookupPath(r, {
                    parent: !0
                }), n = t.node;
                if (!n) throw new i.ErrnoError(44);
                var o = T.basename(r), a = i.mayCreate(n, o);
                if (a) throw new i.ErrnoError(a);
                if (!n.node_ops.symlink) throw new i.ErrnoError(63);
                return n.node_ops.symlink(n, o, e);
            },
            rename (e, r) {
                var t = T.dirname(e), n = T.dirname(r), o = T.basename(e), a = T.basename(r), s, l, d;
                if (s = i.lookupPath(e, {
                    parent: !0
                }), l = s.node, s = i.lookupPath(r, {
                    parent: !0
                }), d = s.node, !l || !d) throw new i.ErrnoError(44);
                if (l.mount !== d.mount) throw new i.ErrnoError(75);
                var _ = i.lookupNode(l, o), v = he.relative(e, n);
                if (v.charAt(0) !== ".") throw new i.ErrnoError(28);
                if (v = he.relative(r, t), v.charAt(0) !== ".") throw new i.ErrnoError(55);
                var g;
                try {
                    g = i.lookupNode(d, a);
                } catch  {}
                if (_ !== g) {
                    var m = i.isDir(_.mode), p = i.mayDelete(l, o, m);
                    if (p) throw new i.ErrnoError(p);
                    if (p = g ? i.mayDelete(d, a, m) : i.mayCreate(d, a), p) throw new i.ErrnoError(p);
                    if (!l.node_ops.rename) throw new i.ErrnoError(63);
                    if (i.isMountpoint(_) || g && i.isMountpoint(g)) throw new i.ErrnoError(10);
                    if (d !== l && (p = i.nodePermissions(l, "w"), p)) throw new i.ErrnoError(p);
                    i.hashRemoveNode(_);
                    try {
                        l.node_ops.rename(_, d, a), _.parent = d;
                    } catch (k) {
                        throw k;
                    } finally{
                        i.hashAddNode(_);
                    }
                }
            },
            rmdir (e) {
                var r = i.lookupPath(e, {
                    parent: !0
                }), t = r.node, n = T.basename(e), o = i.lookupNode(t, n), a = i.mayDelete(t, n, !0);
                if (a) throw new i.ErrnoError(a);
                if (!t.node_ops.rmdir) throw new i.ErrnoError(63);
                if (i.isMountpoint(o)) throw new i.ErrnoError(10);
                t.node_ops.rmdir(t, n), i.destroyNode(o);
            },
            readdir (e) {
                var r = i.lookupPath(e, {
                    follow: !0
                }), t = r.node, n = i.checkOpExists(t.node_ops.readdir, 54);
                return n(t);
            },
            unlink (e) {
                var r = i.lookupPath(e, {
                    parent: !0
                }), t = r.node;
                if (!t) throw new i.ErrnoError(44);
                var n = T.basename(e), o = i.lookupNode(t, n), a = i.mayDelete(t, n, !1);
                if (a) throw new i.ErrnoError(a);
                if (!t.node_ops.unlink) throw new i.ErrnoError(63);
                if (i.isMountpoint(o)) throw new i.ErrnoError(10);
                t.node_ops.unlink(t, n), i.destroyNode(o);
            },
            readlink (e) {
                var r = i.lookupPath(e), t = r.node;
                if (!t) throw new i.ErrnoError(44);
                if (!t.node_ops.readlink) throw new i.ErrnoError(28);
                return t.node_ops.readlink(t);
            },
            stat (e, r) {
                var t = i.lookupPath(e, {
                    follow: !r
                }), n = t.node, o = i.checkOpExists(n.node_ops.getattr, 63);
                return o(n);
            },
            fstat (e) {
                var r = i.getStreamChecked(e), t = r.node, n = r.stream_ops.getattr, o = n ? r : t;
                return n ??= t.node_ops.getattr, i.checkOpExists(n, 63), n(o);
            },
            lstat (e) {
                return i.stat(e, !0);
            },
            doChmod (e, r, t, n) {
                i.doSetAttr(e, r, {
                    mode: t & 4095 | r.mode & -4096,
                    ctime: Date.now(),
                    dontFollow: n
                });
            },
            chmod (e, r, t) {
                var n;
                if (typeof e == "string") {
                    var o = i.lookupPath(e, {
                        follow: !t
                    });
                    n = o.node;
                } else n = e;
                i.doChmod(null, n, r, t);
            },
            lchmod (e, r) {
                i.chmod(e, r, !0);
            },
            fchmod (e, r) {
                var t = i.getStreamChecked(e);
                i.doChmod(t, t.node, r, !1);
            },
            doChown (e, r, t) {
                i.doSetAttr(e, r, {
                    timestamp: Date.now(),
                    dontFollow: t
                });
            },
            chown (e, r, t, n) {
                var o;
                if (typeof e == "string") {
                    var a = i.lookupPath(e, {
                        follow: !n
                    });
                    o = a.node;
                } else o = e;
                i.doChown(null, o, n);
            },
            lchown (e, r, t) {
                i.chown(e, r, t, !0);
            },
            fchown (e, r, t) {
                var n = i.getStreamChecked(e);
                i.doChown(n, n.node, !1);
            },
            doTruncate (e, r, t) {
                if (i.isDir(r.mode)) throw new i.ErrnoError(31);
                if (!i.isFile(r.mode)) throw new i.ErrnoError(28);
                var n = i.nodePermissions(r, "w");
                if (n) throw new i.ErrnoError(n);
                i.doSetAttr(e, r, {
                    size: t,
                    timestamp: Date.now()
                });
            },
            truncate (e, r) {
                if (r < 0) throw new i.ErrnoError(28);
                var t;
                if (typeof e == "string") {
                    var n = i.lookupPath(e, {
                        follow: !0
                    });
                    t = n.node;
                } else t = e;
                i.doTruncate(null, t, r);
            },
            ftruncate (e, r) {
                var t = i.getStreamChecked(e);
                if (r < 0 || (t.flags & 2097155) === 0) throw new i.ErrnoError(28);
                i.doTruncate(t, t.node, r);
            },
            utime (e, r, t) {
                var n = i.lookupPath(e, {
                    follow: !0
                }), o = n.node, a = i.checkOpExists(o.node_ops.setattr, 63);
                a(o, {
                    atime: r,
                    mtime: t
                });
            },
            open (e, r, t = 438) {
                if (e === "") throw new i.ErrnoError(44);
                r = typeof r == "string" ? In(r) : r, r & 64 ? t = t & 4095 | 32768 : t = 0;
                var n, o;
                if (typeof e == "object") n = e;
                else {
                    o = e.endsWith("/");
                    var a = i.lookupPath(e, {
                        follow: !(r & 131072),
                        noent_okay: !0
                    });
                    n = a.node, e = a.path;
                }
                var s = !1;
                if (r & 64) if (n) {
                    if (r & 128) throw new i.ErrnoError(20);
                } else {
                    if (o) throw new i.ErrnoError(31);
                    n = i.mknod(e, t | 511, 0), s = !0;
                }
                if (!n) throw new i.ErrnoError(44);
                if (i.isChrdev(n.mode) && (r &= -513), r & 65536 && !i.isDir(n.mode)) throw new i.ErrnoError(54);
                if (!s) {
                    var l = i.mayOpen(n, r);
                    if (l) throw new i.ErrnoError(l);
                }
                r & 512 && !s && i.truncate(n, 0), r &= -131713;
                var d = i.createStream({
                    node: n,
                    path: i.getPath(n),
                    flags: r,
                    seekable: !0,
                    position: 0,
                    stream_ops: n.stream_ops,
                    ungotten: [],
                    error: !1
                });
                return d.stream_ops.open && d.stream_ops.open(d), s && i.chmod(n, t & 511), f.logReadFiles && !(r & 1) && (e in i.readFiles || (i.readFiles[e] = 1)), d;
            },
            close (e) {
                if (i.isClosed(e)) throw new i.ErrnoError(8);
                e.getdents && (e.getdents = null);
                try {
                    e.stream_ops.close && e.stream_ops.close(e);
                } catch (r) {
                    throw r;
                } finally{
                    i.closeStream(e.fd);
                }
                e.fd = null;
            },
            isClosed (e) {
                return e.fd === null;
            },
            llseek (e, r, t) {
                if (i.isClosed(e)) throw new i.ErrnoError(8);
                if (!e.seekable || !e.stream_ops.llseek) throw new i.ErrnoError(70);
                if (t != 0 && t != 1 && t != 2) throw new i.ErrnoError(28);
                return e.position = e.stream_ops.llseek(e, r, t), e.ungotten = [], e.position;
            },
            read (e, r, t, n, o) {
                if (c(t >= 0), n < 0 || o < 0) throw new i.ErrnoError(28);
                if (i.isClosed(e)) throw new i.ErrnoError(8);
                if ((e.flags & 2097155) === 1) throw new i.ErrnoError(8);
                if (i.isDir(e.node.mode)) throw new i.ErrnoError(31);
                if (!e.stream_ops.read) throw new i.ErrnoError(28);
                var a = typeof o < "u";
                if (!a) o = e.position;
                else if (!e.seekable) throw new i.ErrnoError(70);
                var s = e.stream_ops.read(e, r, t, n, o);
                return a || (e.position += s), s;
            },
            write (e, r, t, n, o, a) {
                if (c(t >= 0), n < 0 || o < 0) throw new i.ErrnoError(28);
                if (i.isClosed(e)) throw new i.ErrnoError(8);
                if ((e.flags & 2097155) === 0) throw new i.ErrnoError(8);
                if (i.isDir(e.node.mode)) throw new i.ErrnoError(31);
                if (!e.stream_ops.write) throw new i.ErrnoError(28);
                e.seekable && e.flags & 1024 && i.llseek(e, 0, 2);
                var s = typeof o < "u";
                if (!s) o = e.position;
                else if (!e.seekable) throw new i.ErrnoError(70);
                var l = e.stream_ops.write(e, r, t, n, o, a);
                return s || (e.position += l), l;
            },
            mmap (e, r, t, n, o) {
                if ((n & 2) !== 0 && (o & 2) === 0 && (e.flags & 2097155) !== 2) throw new i.ErrnoError(2);
                if ((e.flags & 2097155) === 1) throw new i.ErrnoError(2);
                if (!e.stream_ops.mmap) throw new i.ErrnoError(43);
                if (!r) throw new i.ErrnoError(28);
                return e.stream_ops.mmap(e, r, t, n, o);
            },
            msync (e, r, t, n, o) {
                return c(t >= 0), e.stream_ops.msync ? e.stream_ops.msync(e, r, t, n, o) : 0;
            },
            ioctl (e, r, t) {
                if (!e.stream_ops.ioctl) throw new i.ErrnoError(59);
                return e.stream_ops.ioctl(e, r, t);
            },
            readFile (e, r = {}) {
                r.flags = r.flags || 0, r.encoding = r.encoding || "binary", r.encoding !== "utf8" && r.encoding !== "binary" && A(`Invalid encoding type "${r.encoding}"`);
                var t = i.open(e, r.flags), n = i.stat(e), o = n.size, a = new Uint8Array(o);
                return i.read(t, a, 0, o, 0), r.encoding === "utf8" && (a = me(a)), i.close(t), a;
            },
            writeFile (e, r, t = {}) {
                t.flags = t.flags || 577;
                var n = i.open(e, t.flags, t.mode);
                typeof r == "string" && (r = new Uint8Array(_r(r))), ArrayBuffer.isView(r) ? i.write(n, r, 0, r.byteLength, void 0, t.canOwn) : A("Unsupported data type"), i.close(n);
            },
            cwd: ()=>i.currentPath,
            chdir (e) {
                var r = i.lookupPath(e, {
                    follow: !0
                });
                if (r.node === null) throw new i.ErrnoError(44);
                if (!i.isDir(r.node.mode)) throw new i.ErrnoError(54);
                var t = i.nodePermissions(r.node, "x");
                if (t) throw new i.ErrnoError(t);
                i.currentPath = r.path;
            },
            createDefaultDirectories () {
                i.mkdir("/tmp"), i.mkdir("/home"), i.mkdir("/home/web_user");
            },
            createDefaultDevices () {
                i.mkdir("/dev"), i.registerDevice(i.makedev(1, 3), {
                    read: ()=>0,
                    write: (n, o, a, s, l)=>s,
                    llseek: ()=>0
                }), i.mkdev("/dev/null", i.makedev(1, 3)), te.register(i.makedev(5, 0), te.default_tty_ops), te.register(i.makedev(6, 0), te.default_tty1_ops), i.mkdev("/dev/tty", i.makedev(5, 0)), i.mkdev("/dev/tty1", i.makedev(6, 0));
                var e = new Uint8Array(1024), r = 0, t = ()=>(r === 0 && (ur(e), r = e.byteLength), e[--r]);
                i.createDevice("/dev", "random", t), i.createDevice("/dev", "urandom", t), i.mkdir("/dev/shm"), i.mkdir("/dev/shm/tmp");
            },
            createSpecialDirectories () {
                i.mkdir("/proc");
                var e = i.mkdir("/proc/self");
                i.mkdir("/proc/self/fd"), i.mount({
                    mount () {
                        var r = i.createNode(e, "fd", 16895, 73);
                        return r.stream_ops = {
                            llseek: E.stream_ops.llseek
                        }, r.node_ops = {
                            lookup (t, n) {
                                var o = +n, a = i.getStreamChecked(o), s = {
                                    parent: null,
                                    mount: {
                                        mountpoint: "fake"
                                    },
                                    node_ops: {
                                        readlink: ()=>a.path
                                    },
                                    id: o + 1
                                };
                                return s.parent = s, s;
                            },
                            readdir () {
                                return Array.from(i.streams.entries()).filter(([t, n])=>n).map(([t, n])=>t.toString());
                            }
                        }, r;
                    }
                }, {}, "/proc/self/fd");
            },
            createStandardStreams (e, r, t) {
                e ? i.createDevice("/dev", "stdin", e) : i.symlink("/dev/tty", "/dev/stdin"), r ? i.createDevice("/dev", "stdout", null, r) : i.symlink("/dev/tty", "/dev/stdout"), t ? i.createDevice("/dev", "stderr", null, t) : i.symlink("/dev/tty1", "/dev/stderr");
                var n = i.open("/dev/stdin", 0), o = i.open("/dev/stdout", 1), a = i.open("/dev/stderr", 1);
                c(n.fd === 0, `invalid handle for stdin (${n.fd})`), c(o.fd === 1, `invalid handle for stdout (${o.fd})`), c(a.fd === 2, `invalid handle for stderr (${a.fd})`);
            },
            staticInit () {
                i.nameTable = new Array(4096), i.mount(E, {}, "/"), i.createDefaultDirectories(), i.createDefaultDevices(), i.createSpecialDirectories(), i.filesystems = {
                    MEMFS: E
                };
            },
            init (e, r, t) {
                c(!i.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), i.initialized = !0, e ??= f.stdin, r ??= f.stdout, t ??= f.stderr, i.createStandardStreams(e, r, t);
            },
            quit () {
                i.initialized = !1, Er(0);
                for (var e of i.streams)e && i.close(e);
            },
            findObject (e, r) {
                var t = i.analyzePath(e, r);
                return t.exists ? t.object : null;
            },
            analyzePath (e, r) {
                try {
                    var t = i.lookupPath(e, {
                        follow: !r
                    });
                    e = t.path;
                } catch  {}
                var n = {
                    isRoot: !1,
                    exists: !1,
                    error: 0,
                    name: null,
                    path: null,
                    object: null,
                    parentExists: !1,
                    parentPath: null,
                    parentObject: null
                };
                try {
                    var t = i.lookupPath(e, {
                        parent: !0
                    });
                    n.parentExists = !0, n.parentPath = t.path, n.parentObject = t.node, n.name = T.basename(e), t = i.lookupPath(e, {
                        follow: !r
                    }), n.exists = !0, n.path = t.path, n.object = t.node, n.name = t.node.name, n.isRoot = t.path === "/";
                } catch (o) {
                    n.error = o.errno;
                }
                return n;
            },
            createPath (e, r, t, n) {
                e = typeof e == "string" ? e : i.getPath(e);
                for(var o = r.split("/").reverse(); o.length;){
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
            },
            createFile (e, r, t, n, o) {
                var a = T.join2(typeof e == "string" ? e : i.getPath(e), r), s = mr(n, o);
                return i.create(a, s);
            },
            createDataFile (e, r, t, n, o, a) {
                var s = r;
                e && (e = typeof e == "string" ? e : i.getPath(e), s = r ? T.join2(e, r) : e);
                var l = mr(n, o), d = i.create(s, l);
                if (t) {
                    if (typeof t == "string") {
                        for(var _ = new Array(t.length), v = 0, g = t.length; v < g; ++v)_[v] = t.charCodeAt(v);
                        t = _;
                    }
                    i.chmod(d, l | 146);
                    var m = i.open(d, 577);
                    i.write(m, t, 0, t.length, 0, a), i.close(m), i.chmod(d, l);
                }
            },
            createDevice (e, r, t, n) {
                var o = T.join2(typeof e == "string" ? e : i.getPath(e), r), a = mr(!!t, !!n);
                i.createDevice.major ??= 64;
                var s = i.makedev(i.createDevice.major++, 0);
                return i.registerDevice(s, {
                    open (l) {
                        l.seekable = !1;
                    },
                    close (l) {
                        n?.buffer?.length && n(10);
                    },
                    read (l, d, _, v, g) {
                        for(var m = 0, p = 0; p < v; p++){
                            var k;
                            try {
                                k = t();
                            } catch  {
                                throw new i.ErrnoError(29);
                            }
                            if (k === void 0 && m === 0) throw new i.ErrnoError(6);
                            if (k == null) break;
                            m++, d[_ + p] = k;
                        }
                        return m && (l.node.atime = Date.now()), m;
                    },
                    write (l, d, _, v, g) {
                        for(var m = 0; m < v; m++)try {
                            n(d[_ + m]);
                        } catch  {
                            throw new i.ErrnoError(29);
                        }
                        return v && (l.node.mtime = l.node.ctime = Date.now()), m;
                    }
                }), i.mkdev(o, a, s);
            },
            forceLoadFile (e) {
                if (e.isDevice || e.isFolder || e.link || e.contents) return !0;
                if (globalThis.XMLHttpRequest) A("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
                else try {
                    e.contents = be(e.url);
                } catch  {
                    throw new i.ErrnoError(29);
                }
            },
            createLazyFile (e, r, t, n, o) {
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
                        var p = Number(m.getResponseHeader("Content-length")), k, S = (k = m.getResponseHeader("Accept-Ranges")) && k === "bytes", I = (k = m.getResponseHeader("Content-Encoding")) && k === "gzip", x = 1024 * 1024;
                        S || (x = p);
                        var L = (G, we)=>{
                            G > we && A("invalid range (" + G + ", " + we + ") or no bytes requested!"), we > p - 1 && A("only " + p + " bytes available! programmer error!");
                            var W = new XMLHttpRequest();
                            return W.open("GET", t, !1), p !== x && W.setRequestHeader("Range", "bytes=" + G + "-" + we), W.responseType = "arraybuffer", W.overrideMimeType && W.overrideMimeType("text/plain; charset=x-user-defined"), W.send(null), W.status >= 200 && W.status < 300 || W.status === 304 || A("Couldn't load " + t + ". Status: " + W.status), W.response !== void 0 ? new Uint8Array(W.response || []) : _r(W.responseText || "");
                        }, Ie = this;
                        Ie.setDataGetter((G)=>{
                            var we = G * x, W = (G + 1) * x - 1;
                            return W = Math.min(W, p - 1), typeof Ie.chunks[G] > "u" && (Ie.chunks[G] = L(we, W)), typeof Ie.chunks[G] > "u" && A("doXHR failed!"), Ie.chunks[G];
                        }), (I || !p) && (x = p = 1, p = this.getter(0).length, x = p, Q("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = p, this._chunkSize = x, this.lengthKnown = !0;
                    }
                    get length() {
                        return this.lengthKnown || this.cacheLength(), this._length;
                    }
                    get chunkSize() {
                        return this.lengthKnown || this.cacheLength(), this._chunkSize;
                    }
                }
                if (globalThis.XMLHttpRequest) {
                    j || A("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
                    var s = new a(), l = {
                        isDevice: !1,
                        contents: s
                    };
                } else var l = {
                    isDevice: !1,
                    url: t
                };
                var d = i.createFile(e, r, l, n, o);
                l.contents ? d.contents = l.contents : l.url && (d.contents = null, d.url = l.url), Object.defineProperties(d, {
                    usedBytes: {
                        get: function() {
                            return this.contents.length;
                        }
                    }
                });
                var _ = {};
                for (const [g, m] of Object.entries(d.stream_ops))_[g] = (...p)=>(i.forceLoadFile(d), m(...p));
                function v(g, m, p, k, S) {
                    var I = g.node.contents;
                    if (S >= I.length) return 0;
                    var x = Math.min(I.length - S, k);
                    if (c(x >= 0), I.slice) for(var L = 0; L < x; L++)m[p + L] = I[S + L];
                    else for(var L = 0; L < x; L++)m[p + L] = I.get(S + L);
                    return x;
                }
                return _.read = (g, m, p, k, S)=>(i.forceLoadFile(d), v(g, m, p, k, S)), _.mmap = (g, m, p, k, S)=>{
                    i.forceLoadFile(d);
                    var I = ot(m);
                    if (!I) throw new i.ErrnoError(48);
                    return v(g, (u(), O), I, m, p), {
                        ptr: I,
                        allocated: !0
                    };
                }, d.stream_ops = _, d;
            },
            absolutePath () {
                A("FS.absolutePath has been removed; use PATH_FS.resolve instead");
            },
            createFolder () {
                A("FS.createFolder has been removed; use FS.mkdir instead");
            },
            createLink () {
                A("FS.createLink has been removed; use FS.symlink instead");
            },
            joinPath () {
                A("FS.joinPath has been removed; use PATH.join instead");
            },
            mmapAlloc () {
                A("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
            },
            standardizePath () {
                A("FS.standardizePath has been removed; use PATH.normalize instead");
            }
        }, P = {
            DEFAULT_POLLMASK: 5,
            calculateAt (e, r, t) {
                if (T.isAbs(r)) return r;
                var n;
                if (e === -100) n = i.cwd();
                else {
                    var o = P.getStreamFromFD(e);
                    n = o.path;
                }
                if (r.length == 0) {
                    if (!t) throw new i.ErrnoError(44);
                    return n;
                }
                return n + "/" + r;
            },
            writeStat (e, r) {
                (u(), h)[e >> 2] = r.dev, (u(), h)[e + 4 >> 2] = r.mode, (u(), h)[e + 8 >> 2] = r.nlink, (u(), h)[e + 12 >> 2] = r.uid, (u(), h)[e + 16 >> 2] = r.gid, (u(), h)[e + 20 >> 2] = r.rdev, (u(), M)[e + 24 >> 3] = BigInt(r.size), (u(), C)[e + 32 >> 2] = 4096, (u(), C)[e + 36 >> 2] = r.blocks;
                var t = r.atime.getTime(), n = r.mtime.getTime(), o = r.ctime.getTime();
                return (u(), M)[e + 40 >> 3] = BigInt(Math.floor(t / 1e3)), (u(), h)[e + 48 >> 2] = t % 1e3 * 1e3 * 1e3, (u(), M)[e + 56 >> 3] = BigInt(Math.floor(n / 1e3)), (u(), h)[e + 64 >> 2] = n % 1e3 * 1e3 * 1e3, (u(), M)[e + 72 >> 3] = BigInt(Math.floor(o / 1e3)), (u(), h)[e + 80 >> 2] = o % 1e3 * 1e3 * 1e3, (u(), M)[e + 88 >> 3] = BigInt(r.ino), 0;
            },
            writeStatFs (e, r) {
                (u(), h)[e + 4 >> 2] = r.bsize, (u(), h)[e + 60 >> 2] = r.bsize, (u(), M)[e + 8 >> 3] = BigInt(r.blocks), (u(), M)[e + 16 >> 3] = BigInt(r.bfree), (u(), M)[e + 24 >> 3] = BigInt(r.bavail), (u(), M)[e + 32 >> 3] = BigInt(r.files), (u(), M)[e + 40 >> 3] = BigInt(r.ffree), (u(), h)[e + 48 >> 2] = r.fsid, (u(), h)[e + 64 >> 2] = r.flags, (u(), h)[e + 56 >> 2] = r.namelen;
            },
            doMsync (e, r, t, n, o) {
                if (!i.isFile(r.node.mode)) throw new i.ErrnoError(43);
                if (n & 2) return 0;
                var a = (u(), U).slice(e, e + t);
                i.msync(r, a, o, t, n);
            },
            getStreamFromFD (e) {
                var r = i.getStreamChecked(e);
                return r;
            },
            varargs: void 0,
            getStr (e) {
                var r = X(e);
                return r;
            }
        };
        function dt(e, r, t) {
            if (y) return N(3, 0, 1, e, r, t);
            P.varargs = t;
            try {
                var n = P.getStreamFromFD(e);
                switch(r){
                    case 0:
                        {
                            var o = Ue();
                            if (o < 0) return -28;
                            for(; i.streams[o];)o++;
                            var a;
                            return a = i.dupStream(n, o), a.fd;
                        }
                    case 1:
                    case 2:
                        return 0;
                    case 3:
                        return n.flags;
                    case 4:
                        {
                            var o = Ue();
                            return n.flags |= o, 0;
                        }
                    case 12:
                        {
                            var o = ve(), s = 0;
                            return (u(), ie)[o + s >> 1] = 2, 0;
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
        function ct(e, r) {
            if (y) return N(4, 0, 1, e, r);
            try {
                return P.writeStat(r, i.fstat(e));
            } catch (t) {
                if (typeof i > "u" || t.name !== "ErrnoError") throw t;
                return -t.errno;
            }
        }
        function ut(e, r, t) {
            if (y) return N(5, 0, 1, e, r, t);
            P.varargs = t;
            try {
                var n = P.getStreamFromFD(e);
                switch(r){
                    case 21509:
                        return n.tty ? 0 : -59;
                    case 21505:
                        {
                            if (!n.tty) return -59;
                            if (n.tty.ops.ioctl_tcgets) {
                                var o = n.tty.ops.ioctl_tcgets(n), a = ve();
                                (u(), C)[a >> 2] = o.c_iflag || 0, (u(), C)[a + 4 >> 2] = o.c_oflag || 0, (u(), C)[a + 8 >> 2] = o.c_cflag || 0, (u(), C)[a + 12 >> 2] = o.c_lflag || 0;
                                for(var s = 0; s < 32; s++)(u(), O)[a + s + 17] = o.c_cc[s] || 0;
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
                    case 21508:
                        {
                            if (!n.tty) return -59;
                            if (n.tty.ops.ioctl_tcsets) {
                                for(var a = ve(), l = (u(), C)[a >> 2], d = (u(), C)[a + 4 >> 2], _ = (u(), C)[a + 8 >> 2], v = (u(), C)[a + 12 >> 2], g = [], s = 0; s < 32; s++)g.push((u(), O)[a + s + 17]);
                                return n.tty.ops.ioctl_tcsets(n.tty, r, {
                                    c_iflag: l,
                                    c_oflag: d,
                                    c_cflag: _,
                                    c_lflag: v,
                                    c_cc: g
                                });
                            }
                            return 0;
                        }
                    case 21519:
                        {
                            if (!n.tty) return -59;
                            var a = ve();
                            return (u(), C)[a >> 2] = 0, 0;
                        }
                    case 21520:
                        return n.tty ? -28 : -59;
                    case 21537:
                    case 21531:
                        {
                            var a = ve();
                            return i.ioctl(n, r, a);
                        }
                    case 21523:
                        {
                            if (!n.tty) return -59;
                            if (n.tty.ops.ioctl_tiocgwinsz) {
                                var m = n.tty.ops.ioctl_tiocgwinsz(n.tty), a = ve();
                                (u(), ie)[a >> 1] = m[0], (u(), ie)[a + 2 >> 1] = m[1];
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
        function ft(e, r) {
            if (y) return N(6, 0, 1, e, r);
            try {
                return e = P.getStr(e), P.writeStat(r, i.lstat(e));
            } catch (t) {
                if (typeof i > "u" || t.name !== "ErrnoError") throw t;
                return -t.errno;
            }
        }
        function _t(e, r, t, n) {
            if (y) return N(7, 0, 1, e, r, t, n);
            try {
                r = P.getStr(r);
                var o = n & 256, a = n & 4096;
                return n = n & -6401, c(!n, `unknown flags in __syscall_newfstatat: ${n}`), r = P.calculateAt(e, r, a), P.writeStat(t, o ? i.lstat(r) : i.stat(r));
            } catch (s) {
                if (typeof i > "u" || s.name !== "ErrnoError") throw s;
                return -s.errno;
            }
        }
        function mt(e, r, t, n) {
            if (y) return N(8, 0, 1, e, r, t, n);
            P.varargs = n;
            try {
                r = P.getStr(r), r = P.calculateAt(e, r);
                var o = n ? Ue() : 0;
                return i.open(r, t, o).fd;
            } catch (a) {
                if (typeof i > "u" || a.name !== "ErrnoError") throw a;
                return -a.errno;
            }
        }
        function vt(e, r) {
            if (y) return N(9, 0, 1, e, r);
            try {
                return e = P.getStr(e), P.writeStat(r, i.stat(e));
            } catch (t) {
                if (typeof i > "u" || t.name !== "ErrnoError") throw t;
                return -t.errno;
            }
        }
        var xn = ()=>A("native code called abort()"), $ = (e)=>{
            for(var r = "";;){
                var t = (u(), U)[e++];
                if (!t) return r;
                r += String.fromCharCode(t);
            }
        }, pe = {}, ge = {}, $e = {}, Ln = class extends Error {
            constructor(r){
                super(r), this.name = "BindingError";
            }
        }, B = (e)=>{
            throw new Ln(e);
        };
        function Un(e, r, t = {}) {
            var n = r.name;
            if (e || B(`type "${n}" must have a positive integer typeid pointer`), ge.hasOwnProperty(e)) {
                if (t.ignoreDuplicateRegistrations) return;
                B(`Cannot register type '${n}' twice`);
            }
            if (ge[e] = r, delete $e[e], pe.hasOwnProperty(e)) {
                var o = pe[e];
                delete pe[e], o.forEach((a)=>a());
            }
        }
        function H(e, r, t = {}) {
            return Un(e, r, t);
        }
        var ht = (e, r, t)=>{
            switch(r){
                case 1:
                    return t ? (n)=>(u(), O)[n] : (n)=>(u(), U)[n];
                case 2:
                    return t ? (n)=>(u(), ie)[n >> 1] : (n)=>(u(), Fe)[n >> 1];
                case 4:
                    return t ? (n)=>(u(), C)[n >> 2] : (n)=>(u(), h)[n >> 2];
                case 8:
                    return t ? (n)=>(u(), M)[n >> 3] : (n)=>(u(), Or)[n >> 3];
                default:
                    throw new TypeError(`invalid integer width (${r}): ${e}`);
            }
        }, Be = (e)=>{
            if (e === null) return "null";
            var r = typeof e;
            return r === "object" || r === "array" || r === "function" ? e.toString() : "" + e;
        }, pt = (e, r, t, n)=>{
            if (r < t || r > n) throw new TypeError(`Passing a number "${Be(r)}" from JS side to C/C++ side to an argument of type "${e}", which is outside the valid range [${t}, ${n}]!`);
        }, $n = (e, r, t, n, o)=>{
            r = $(r);
            const a = n === 0n;
            let s = (l)=>l;
            if (a) {
                const l = t * 8;
                s = (d)=>BigInt.asUintN(l, d), o = s(o);
            }
            H(e, {
                name: r,
                fromWireType: s,
                toWireType: (l, d)=>{
                    if (typeof d == "number") d = BigInt(d);
                    else if (typeof d != "bigint") throw new TypeError(`Cannot convert "${Be(d)}" to ${this.name}`);
                    return pt(r, d, n, o), d;
                },
                readValueFromPointer: ht(r, t, !a),
                destructorFunction: null
            });
        }, Bn = (e, r, t, n)=>{
            r = $(r), H(e, {
                name: r,
                fromWireType: function(o) {
                    return !!o;
                },
                toWireType: function(o, a) {
                    return a ? t : n;
                },
                readValueFromPointer: function(o) {
                    return this.fromWireType((u(), U)[o]);
                },
                destructorFunction: null
            });
        }, gt = [], q = [
            0,
            1,
            ,
            1,
            null,
            1,
            !0,
            1,
            !1,
            1
        ], jn = (e)=>{
            e > 9 && --q[e + 1] === 0 && (c(q[e] !== void 0, "Decref for unallocated handle."), q[e] = void 0, gt.push(e));
        }, yt = {
            toValue: (e)=>(e || B(`Cannot use deleted val. handle = ${e}`), c(e === 2 || q[e] !== void 0 && e % 2 === 0, `invalid handle: ${e}`), q[e]),
            toHandle: (e)=>{
                switch(e){
                    case void 0:
                        return 2;
                    case null:
                        return 4;
                    case !0:
                        return 6;
                    case !1:
                        return 8;
                    default:
                        {
                            const r = gt.pop() || q.length;
                            return q[r] = e, q[r + 1] = 1, r;
                        }
                }
            }
        };
        function vr(e) {
            return this.fromWireType((u(), h)[e >> 2]);
        }
        var zn = {
            name: "emscripten::val",
            fromWireType: (e)=>{
                var r = yt.toValue(e);
                return jn(e), r;
            },
            toWireType: (e, r)=>yt.toHandle(r),
            readValueFromPointer: vr,
            destructorFunction: null
        }, Hn = (e)=>H(e, zn), Vn = (e, r)=>{
            switch(r){
                case 4:
                    return function(t) {
                        return this.fromWireType((u(), Nr)[t >> 2]);
                    };
                case 8:
                    return function(t) {
                        return this.fromWireType((u(), Ae)[t >> 3]);
                    };
                default:
                    throw new TypeError(`invalid float width (${r}): ${e}`);
            }
        }, Gn = (e, r, t)=>{
            r = $(r), H(e, {
                name: r,
                fromWireType: (n)=>n,
                toWireType: (n, o)=>{
                    if (typeof o != "number" && typeof o != "boolean") throw new TypeError(`Cannot convert ${Be(o)} to ${this.name}`);
                    return o;
                },
                readValueFromPointer: Vn(r, t),
                destructorFunction: null
            });
        }, Et = (e, r)=>Object.defineProperty(r, "name", {
                value: e
            }), Kn = (e)=>{
            for(; e.length;){
                var r = e.pop(), t = e.pop();
                t(r);
            }
        };
        function wt(e) {
            for(var r = 1; r < e.length; ++r)if (e[r] !== null && e[r].destructorFunction === void 0) return !0;
            return !1;
        }
        function Xn(e, r, t, n, o) {
            if (e < r || e > t) {
                var a = r == t ? r : `${r} to ${t}`;
                o(`function ${n} called with ${e} arguments, expected ${a}`);
            }
        }
        function qn(e, r, t, n) {
            for(var o = wt(e), a = e.length - 2, s = [], l = [
                "fn"
            ], d = 0; d < a; ++d)s.push(`arg${d}`), l.push(`arg${d}Wired`);
            s = s.join(","), l = l.join(",");
            var _ = `return function (${s}) {
`;
            _ += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, o && (_ += `var destructors = [];
`);
            for(var v = o ? "destructors" : "null", g = [
                "humanName",
                "throwBindingError",
                "invoker",
                "fn",
                "runDestructors",
                "fromRetWire",
                "toClassParamWire"
            ], d = 0; d < a; ++d){
                var m = `toArg${d}Wire`;
                _ += `var arg${d}Wired = ${m}(${v}, arg${d});
`, g.push(m);
            }
            _ += (t || n ? "var rv = " : "") + `invoker(${l});
`;
            var p = t ? "rv" : "";
            if (_ += `function onDone(${p}) {
`, o) _ += `runDestructors(destructors);
`;
            else for(var d = 2; d < e.length; ++d){
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
        function Yn(e) {
            for(var r = e.length - 2, t = e.length - 1; t >= 2 && e[t].optional; --t)r--;
            return r;
        }
        function Jn(e, r, t, n, o, a) {
            var s = r.length;
            s < 2 && B("argTypes array size mismatch! Must at least get return value and 'this' types!");
            for(var l = r[1] !== null && t !== null, d = wt(r), _ = !r[0].isVoid, v = s - 2, g = Yn(r), m = r[0], p = r[1], k = [
                e,
                B,
                n,
                o,
                Kn,
                m.fromWireType.bind(m),
                p?.toWireType.bind(p)
            ], S = 2; S < s; ++S){
                var I = r[S];
                k.push(I.toWireType.bind(I));
            }
            if (!d) for(var S = 2; S < r.length; ++S)r[S].destructorFunction !== null && k.push(r[S].destructorFunction);
            k.push(Xn, g, v);
            var L = qn(r, l, _, a)(...k);
            return Et(e, L);
        }
        var Zn = (e, r, t)=>{
            if (e[r].overloadTable === void 0) {
                var n = e[r];
                e[r] = function(...o) {
                    return e[r].overloadTable.hasOwnProperty(o.length) || B(`Function '${t}' called with an invalid number of arguments (${o.length}) - expects one of (${e[r].overloadTable})!`), e[r].overloadTable[o.length].apply(this, o);
                }, e[r].overloadTable = [], e[r].overloadTable[n.argCount] = n;
            }
        }, Qn = (e, r, t)=>{
            f.hasOwnProperty(e) ? ((t === void 0 || f[e].overloadTable !== void 0 && f[e].overloadTable[t] !== void 0) && B(`Cannot register public name '${e}' twice`), Zn(f, e, e), f[e].overloadTable.hasOwnProperty(t) && B(`Cannot register multiple overloads of a function with the same number of arguments (${t})!`), f[e].overloadTable[t] = r) : (f[e] = r, f[e].argCount = t);
        }, ei = (e, r)=>{
            for(var t = [], n = 0; n < e; n++)t.push((u(), h)[r + n * 4 >> 2]);
            return t;
        }, ri = class extends Error {
            constructor(r){
                super(r), this.name = "InternalError";
            }
        }, bt = (e)=>{
            throw new ri(e);
        }, ti = (e, r, t)=>{
            f.hasOwnProperty(e) || bt("Replacing nonexistent public symbol"), f[e].overloadTable !== void 0 && t !== void 0 ? f[e].overloadTable[t] = r : (f[e] = r, f[e].argCount = t);
        }, ni = (e, r, t = !1)=>{
            e = $(e);
            function n() {
                var a = Jr(r);
                return t && (a = WebAssembly.promising(a)), a;
            }
            var o = n();
            return typeof o != "function" && B(`unknown function pointer with signature ${e}: ${r}`), o;
        };
        class ii extends Error {
        }
        var oi = (e)=>{
            var r = jt(e), t = $(r);
            return J(r), t;
        }, ai = (e, r)=>{
            var t = [], n = {};
            function o(a) {
                if (!n[a] && !ge[a]) {
                    if ($e[a]) {
                        $e[a].forEach(o);
                        return;
                    }
                    t.push(a), n[a] = !0;
                }
            }
            throw r.forEach(o), new ii(`${e}: ` + t.map(oi).join([
                ", "
            ]));
        }, si = (e, r, t)=>{
            e.forEach((l)=>$e[l] = r);
            function n(l) {
                var d = t(l);
                d.length !== e.length && bt("Mismatched type converter count");
                for(var _ = 0; _ < e.length; ++_)H(e[_], d[_]);
            }
            var o = new Array(r.length), a = [], s = 0;
            for (let [l, d] of r.entries())ge.hasOwnProperty(d) ? o[l] = ge[d] : (a.push(d), pe.hasOwnProperty(d) || (pe[d] = []), pe[d].push(()=>{
                o[l] = ge[d], ++s, s === a.length && n(o);
            }));
            a.length === 0 && n(o);
        }, li = (e)=>{
            e = e.trim();
            const r = e.indexOf("(");
            return r === -1 ? e : (c(e.endsWith(")"), "Parentheses for argument names should match."), e.slice(0, r));
        }, di = (e, r, t, n, o, a, s, l)=>{
            var d = ei(r, t);
            e = $(e), e = li(e), o = ni(n, o, s), Qn(e, function() {
                ai(`Cannot call ${e} due to unbound types`, d);
            }, r - 1), si([], d, (_)=>{
                var v = [
                    _[0],
                    null
                ].concat(_.slice(1));
                return ti(e, Jn(e, v, null, o, a, s), r - 1), [];
            });
        }, ci = (e, r, t, n, o)=>{
            r = $(r);
            const a = n === 0;
            let s = (d)=>d;
            if (a) {
                var l = 32 - 8 * t;
                s = (d)=>d << l >>> l, o = s(o);
            }
            H(e, {
                name: r,
                fromWireType: s,
                toWireType: (d, _)=>{
                    if (typeof _ != "number" && typeof _ != "boolean") throw new TypeError(`Cannot convert "${Be(_)}" to ${r}`);
                    return pt(r, _, n, o), _;
                },
                readValueFromPointer: ht(r, t, n !== 0),
                destructorFunction: null
            });
        }, ui = (e, r, t)=>{
            var n = [
                Int8Array,
                Uint8Array,
                Int16Array,
                Uint16Array,
                Int32Array,
                Uint32Array,
                Float32Array,
                Float64Array,
                BigInt64Array,
                BigUint64Array
            ], o = n[r];
            function a(s) {
                var l = (u(), h)[s >> 2], d = (u(), h)[s + 4 >> 2];
                return new o((u(), O).buffer, d, l);
            }
            t = $(t), H(e, {
                name: t,
                fromWireType: a,
                readValueFromPointer: a
            }, {
                ignoreDuplicateRegistrations: !0
            });
        }, Y = (e, r, t)=>(c(typeof t == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), nt(e, (u(), U), r, t)), fi = (e, r)=>{
            r = $(r), H(e, {
                name: r,
                fromWireType (t) {
                    var n = (u(), h)[t >> 2], o = t + 4, a;
                    return a = X(o, n, !0), J(t), a;
                },
                toWireType (t, n) {
                    n instanceof ArrayBuffer && (n = new Uint8Array(n));
                    var o, a = typeof n == "string";
                    a || ArrayBuffer.isView(n) && n.BYTES_PER_ELEMENT == 1 || B("Cannot pass non-string to std::string"), a ? o = se(n) : o = n.length;
                    var s = Ke(4 + o + 1), l = s + 4;
                    return (u(), h)[s >> 2] = o, a ? Y(n, l, o + 1) : (u(), U).set(n, l), t !== null && t.push(J, s), s;
                },
                readValueFromPointer: vr,
                destructorFunction (t) {
                    J(t);
                }
            });
        }, St = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, _i = (e, r, t)=>{
            c(e % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
            var n = e >> 1, o = et((u(), Fe), n, r / 2, t);
            if (o - n > 16 && St) return St.decode((u(), Fe).slice(n, o));
            for(var a = "", s = n; s < o; ++s){
                var l = (u(), Fe)[s];
                a += String.fromCharCode(l);
            }
            return a;
        }, mi = (e, r, t)=>{
            if (c(r % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), c(typeof t == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 2) return 0;
            t -= 2;
            for(var n = r, o = t < e.length * 2 ? t / 2 : e.length, a = 0; a < o; ++a){
                var s = e.charCodeAt(a);
                (u(), ie)[r >> 1] = s, r += 2;
            }
            return (u(), ie)[r >> 1] = 0, r - n;
        }, vi = (e)=>e.length * 2, hi = (e, r, t)=>{
            c(e % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
            for(var n = "", o = e >> 2, a = 0; !(a >= r / 4); a++){
                var s = (u(), h)[o + a];
                if (!s && !t) break;
                n += String.fromCodePoint(s);
            }
            return n;
        }, pi = (e, r, t)=>{
            if (c(r % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), c(typeof t == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 4) return 0;
            for(var n = r, o = n + t - 4, a = 0; a < e.length; ++a){
                var s = e.codePointAt(a);
                if (s > 65535 && a++, (u(), C)[r >> 2] = s, r += 4, r + 4 > o) break;
            }
            return (u(), C)[r >> 2] = 0, r - n;
        }, gi = (e)=>{
            for(var r = 0, t = 0; t < e.length; ++t){
                var n = e.codePointAt(t);
                n > 65535 && t++, r += 4;
            }
            return r;
        }, yi = (e, r, t)=>{
            t = $(t);
            var n, o, a;
            r === 2 ? (n = _i, o = mi, a = vi) : (c(r === 4, "only 2-byte and 4-byte strings are currently supported"), n = hi, o = pi, a = gi), H(e, {
                name: t,
                fromWireType: (s)=>{
                    var l = (u(), h)[s >> 2], d = n(s + 4, l * r, !0);
                    return J(s), d;
                },
                toWireType: (s, l)=>{
                    typeof l != "string" && B(`Cannot pass non-string to C++ string type ${t}`);
                    var d = a(l), _ = Ke(4 + d + r);
                    return (u(), h)[_ >> 2] = d / r, o(l, _ + 4, d + r), s !== null && s.push(J, _), _;
                },
                readValueFromPointer: vr,
                destructorFunction (s) {
                    J(s);
                }
            });
        }, Ei = (e, r)=>{
            r = $(r), H(e, {
                isVoid: !0,
                name: r,
                fromWireType: ()=>{},
                toWireType: (t, n)=>{}
            });
        }, wi = (e)=>{
            wr(e, !j, 1, !Me, 65536, !1), w.threadInitTLS();
        }, kt = (e)=>{
            if (e instanceof Ur || e == "unwind") return ce;
            Te(), e instanceof WebAssembly.RuntimeError && kr() <= 0 && b("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), Ze(1, e);
        }, bi = ()=>{
            if (!Le()) try {
                if (y) {
                    ye() && Sr(ce);
                    return;
                }
                lr(ce);
            } catch (e) {
                kt(e);
            }
        }, Tt = (e)=>{
            if (de) {
                b("user callback triggered after runtime exited or application aborted.  Ignoring.");
                return;
            }
            try {
                e(), bi();
            } catch (r) {
                kt(r);
            }
        }, hr = (e)=>{
            if (Atomics.waitAsync) {
                var r = Atomics.waitAsync((u(), C), e >> 2, e);
                c(r.async), r.value.then(je);
                var t = e + 128;
                Atomics.store((u(), C), t >> 2, 1);
            }
        }, je = ()=>Tt(()=>{
                var e = ye();
                e && (hr(e), qt());
            }), Si = (e, r)=>{
            if (e == r) setTimeout(je);
            else if (y) postMessage({
                targetThread: e,
                cmd: "checkMailbox"
            });
            else {
                var t = w.pthreads[e];
                if (!t) {
                    b(`Cannot send message to thread with ID ${e}, unknown thread ID!`);
                    return;
                }
                t.postMessage({
                    cmd: "checkMailbox"
                });
            }
        }, ze = [], ki = (e, r, t, n, o)=>{
            n /= 2, ze.length = n;
            for(var a = o >> 3, s = 0; s < n; s++)(u(), M)[a + 2 * s] ? ze[s] = (u(), M)[a + 2 * s + 1] : ze[s] = (u(), Ae)[a + 2 * s + 1];
            var l = r ? yr[r] : no[e];
            c(!(e && r)), c(l.length == n, "Call args mismatch in _emscripten_receive_on_main_thread_js"), w.currentProxiedOperationCallerThread = t;
            var d = l(...ze);
            return w.currentProxiedOperationCallerThread = 0, c(typeof d != "bigint"), d;
        }, Ti = (e)=>{
            y ? postMessage({
                cmd: "cleanupThread",
                thread: e
            }) : Br(e);
        }, Fi = (e)=>{
            R && w.pthreads[e].ref();
        }, Ai = 9007199254740992, Pi = -9007199254740992, pr = (e)=>e < Pi || e > Ai ? NaN : Number(e);
        function Ft(e, r, t, n, o, a, s) {
            if (y) return N(10, 0, 1, e, r, t, n, o, a, s);
            o = pr(o);
            try {
                c(!isNaN(o));
                var l = P.getStreamFromFD(n), d = i.mmap(l, e, o, r, t), _ = d.ptr;
                return (u(), C)[a >> 2] = d.allocated, (u(), h)[s >> 2] = _, 0;
            } catch (v) {
                if (typeof i > "u" || v.name !== "ErrnoError") throw v;
                return -v.errno;
            }
        }
        function At(e, r, t, n, o, a) {
            if (y) return N(11, 0, 1, e, r, t, n, o, a);
            a = pr(a);
            try {
                var s = P.getStreamFromFD(o);
                t & 2 && P.doMsync(e, s, r, n, a);
            } catch (l) {
                if (typeof i > "u" || l.name !== "ErrnoError") throw l;
                return -l.errno;
            }
        }
        var Ci = (e, r, t, n)=>{
            var o = (new Date()).getFullYear(), a = new Date(o, 0, 1), s = new Date(o, 6, 1), l = a.getTimezoneOffset(), d = s.getTimezoneOffset(), _ = Math.max(l, d);
            (u(), h)[e >> 2] = _ * 60, (u(), C)[r >> 2] = +(l != d);
            var v = (p)=>{
                var k = p >= 0 ? "-" : "+", S = Math.abs(p), I = String(Math.floor(S / 60)).padStart(2, "0"), x = String(S % 60).padStart(2, "0");
                return `UTC${k}${I}${x}`;
            }, g = v(l), m = v(d);
            c(g), c(m), c(se(g) <= 16, `timezone name truncated to fit in TZNAME_MAX (${g})`), c(se(m) <= 16, `timezone name truncated to fit in TZNAME_MAX (${m})`), d < l ? (Y(g, t, 17), Y(m, n, 17)) : (Y(g, n, 17), Y(m, t, 17));
        }, Pt = ()=>performance.timeOrigin + performance.now(), Ii = ()=>Date.now(), Mi = (e)=>e >= 0 && e <= 3;
        function Di(e, r, t) {
            if (!Mi(e)) return 28;
            var n;
            e === 0 ? n = Ii() : n = Pt();
            var o = Math.round(n * 1e3 * 1e3);
            return (u(), M)[t >> 3] = BigInt(o), 0;
        }
        var He = [], Ri = (e, r)=>{
            c(Array.isArray(He)), c(r % 16 == 0), He.length = 0;
            for(var t; t = (u(), U)[e++];){
                var n = String.fromCharCode(t), o = [
                    "d",
                    "f",
                    "i",
                    "p"
                ];
                o.push("j"), c(o.includes(n), `Invalid character ${t}("${n}") in readEmAsmArgs! Use only [${o}], and do not specify "v" for void return argument.`);
                var a = t != 105;
                a &= t != 112, r += a && r % 8 ? 4 : 0, He.push(t == 112 ? (u(), h)[r >> 2] : t == 106 ? (u(), M)[r >> 3] : t == 105 ? (u(), C)[r >> 2] : (u(), Ae)[r >> 3]), r += a ? 8 : 4;
            }
            return He;
        }, Ni = (e, r, t)=>{
            var n = Ri(r, t);
            return c(yr.hasOwnProperty(e), `No EM_ASM constant found at address ${e}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), yr[e](...n);
        }, Oi = (e, r, t)=>Ni(e, r, t), Wi = ()=>{
            R || j || re("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
        }, xi = (e, r)=>b(X(e, r)), Ct = ()=>{
            _e += 1;
        }, Li = ()=>{
            throw Ct(), "unwind";
        }, It = ()=>2147483648, Ui = ()=>It(), $i = ()=>R ? K("os").cpus().length : navigator.hardwareConcurrency, le = {}, Mt = (e)=>{
            var r = se(e) + 1, t = Ke(r);
            return t && Y(e, t, r), t;
        }, Ve = (e)=>{
            var r;
            if (r = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(e)) return +r[1];
            if (r = /\bwasm-function\[(\d+)\]:(\d+)/.exec(e)) re("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
            else if (r = /:(\d+):\d+(?:\)|$)/.exec(e)) return 2147483648 | +r[1];
            return 0;
        }, Dt = (e)=>{
            for (var r of e){
                var t = Ve(r);
                t && (le[t] = r);
            }
        }, Rt = ()=>new Error().stack.toString(), Bi = ()=>{
            var e = Rt().split(`
`);
            return e[0] == "Error" && e.shift(), Dt(e), le.last_addr = Ve(e[3]), le.last_stack = e, le.last_addr;
        }, Ge = (e)=>{
            var r = le[e];
            if (!r) return 0;
            var t, n;
            if (n = /^\s+at .*\.wasm\.(.*) \(.*\)$/.exec(r)) t = n[1];
            else if (n = /^\s+at (.*) \(.*\)$/.exec(r)) t = n[1];
            else if (n = /^(.+?)@/.exec(r)) t = n[1];
            else return 0;
            return J(Ge.ret ?? 0), Ge.ret = Mt(t), Ge.ret;
        }, ji = (e)=>{
            var r = z.buffer.byteLength, t = (e - r + 65535) / 65536 | 0;
            try {
                return z.grow(t), xe(), 1;
            } catch (n) {
                b(`growMemory: Attempted to grow heap from ${r} bytes to ${e} bytes, but got error: ${n}`);
            }
        }, zi = (e)=>{
            var r = (u(), U).length;
            if (e >>>= 0, e <= r) return !1;
            var t = It();
            if (e > t) return b(`Cannot enlarge memory, requested ${e} bytes, but the limit is ${t} bytes!`), !1;
            for(var n = 1; n <= 4; n *= 2){
                var o = r * (1 + 0.2 / n);
                o = Math.min(o, e + 100663296);
                var a = Math.min(t, it(Math.max(e, o), 65536)), s = ji(a);
                if (s) return !0;
            }
            return b(`Failed to grow the heap from ${r} bytes to ${a} bytes, not enough memory!`), !1;
        }, Hi = (e, r, t)=>{
            var n;
            le.last_addr == e ? n = le.last_stack : (n = Rt().split(`
`), n[0] == "Error" && n.shift(), Dt(n));
            for(var o = 3; n[o] && Ve(n[o]) != e;)++o;
            for(var a = 0; a < t && n[a + o]; ++a)(u(), C)[r + a * 4 >> 2] = Ve(n[a + o]);
            return a;
        }, gr = {}, Vi = ()=>Je || "./this.program", Ce = ()=>{
            if (!Ce.strings) {
                var e = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", r = {
                    USER: "web_user",
                    LOGNAME: "web_user",
                    PATH: "/",
                    PWD: "/",
                    HOME: "/home/web_user",
                    LANG: e,
                    _: Vi()
                };
                for(var t in gr)gr[t] === void 0 ? delete r[t] : r[t] = gr[t];
                var n = [];
                for(var t in r)n.push(`${t}=${r[t]}`);
                Ce.strings = n;
            }
            return Ce.strings;
        };
        function Nt(e, r) {
            if (y) return N(12, 0, 1, e, r);
            var t = 0, n = 0;
            for (var o of Ce()){
                var a = r + t;
                (u(), h)[e + n >> 2] = a, t += Y(o, a, 1 / 0) + 1, n += 4;
            }
            return 0;
        }
        function Ot(e, r) {
            if (y) return N(13, 0, 1, e, r);
            var t = Ce();
            (u(), h)[e >> 2] = t.length;
            var n = 0;
            for (var o of t)n += se(o) + 1;
            return (u(), h)[r >> 2] = n, 0;
        }
        function Wt(e) {
            if (y) return N(14, 0, 1, e);
            try {
                var r = P.getStreamFromFD(e);
                return i.close(r), 0;
            } catch (t) {
                if (typeof i > "u" || t.name !== "ErrnoError") throw t;
                return t.errno;
            }
        }
        var Gi = (e, r, t, n)=>{
            for(var o = 0, a = 0; a < t; a++){
                var s = (u(), h)[r >> 2], l = (u(), h)[r + 4 >> 2];
                r += 8;
                var d = i.read(e, (u(), O), s, l, n);
                if (d < 0) return -1;
                if (o += d, d < l) break;
            }
            return o;
        };
        function xt(e, r, t, n) {
            if (y) return N(15, 0, 1, e, r, t, n);
            try {
                var o = P.getStreamFromFD(e), a = Gi(o, r, t);
                return (u(), h)[n >> 2] = a, 0;
            } catch (s) {
                if (typeof i > "u" || s.name !== "ErrnoError") throw s;
                return s.errno;
            }
        }
        function Lt(e, r, t, n) {
            if (y) return N(16, 0, 1, e, r, t, n);
            r = pr(r);
            try {
                if (isNaN(r)) return 61;
                var o = P.getStreamFromFD(e);
                return i.llseek(o, r, t), (u(), M)[n >> 3] = BigInt(o.position), o.getdents && r === 0 && t === 0 && (o.getdents = null), 0;
            } catch (a) {
                if (typeof i > "u" || a.name !== "ErrnoError") throw a;
                return a.errno;
            }
        }
        var Ki = (e, r, t, n)=>{
            for(var o = 0, a = 0; a < t; a++){
                var s = (u(), h)[r >> 2], l = (u(), h)[r + 4 >> 2];
                r += 8;
                var d = i.write(e, (u(), O), s, l, n);
                if (d < 0) return -1;
                if (o += d, d < l) break;
            }
            return o;
        };
        function Ut(e, r, t, n) {
            if (y) return N(17, 0, 1, e, r, t, n);
            try {
                var o = P.getStreamFromFD(e), a = Ki(o, r, t);
                return (u(), h)[n >> 2] = a, 0;
            } catch (s) {
                if (typeof i > "u" || s.name !== "ErrnoError") throw s;
                return s.errno;
            }
        }
        function Xi(e, r) {
            try {
                return ur((u(), U).subarray(e, e + r)), 0;
            } catch (t) {
                if (typeof i > "u" || t.name !== "ErrnoError") throw t;
                return t.errno;
            }
        }
        var qi = ()=>{
            c(_e > 0), _e -= 1;
        }, V = {
            instrumentWasmImports (e) {
                c("Suspending" in WebAssembly, "JSPI not supported by current environment. Perhaps it needs to be enabled via flags?");
                var r = /^(invoke_.*|__asyncjs__.*)$/;
                for (let [t, n] of Object.entries(e))typeof n == "function" && (n.isAsync || r.test(t)) && (e[t] = n = new WebAssembly.Suspending(n));
            },
            instrumentFunction (e) {
                var r = (...t)=>e(...t);
                return r = Et(`__asyncify_wrapper_${e.name}`, r), r;
            },
            instrumentWasmExports (e) {
                var r = /^(solve_model|validate_model|main|__main_argc_argv)$/;
                V.asyncExports = new Set();
                var t = {};
                for (let [o, a] of Object.entries(e))if (typeof a == "function") {
                    r.test(o) && (V.asyncExports.add(a), a = V.makeAsyncFunction(a));
                    var n = V.instrumentFunction(a);
                    t[o] = n;
                } else t[o] = a;
                return t;
            },
            asyncExports: null,
            isAsyncExport (e) {
                return V.asyncExports?.has(e);
            },
            handleAsync: async (e)=>{
                Ct();
                try {
                    return await e();
                } finally{
                    qi();
                }
            },
            handleSleep: (e)=>V.handleAsync(()=>new Promise(e)),
            makeAsyncFunction (e) {
                return WebAssembly.promising(e);
            }
        }, Yi = (e)=>{
            var r = f["_" + e];
            return c(r, "Cannot call unknown function " + e + ", make sure it is exported"), r;
        }, Ji = (e, r)=>{
            c(e.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (u(), O).set(e, r);
        }, Zi = (e)=>{
            var r = se(e) + 1, t = ar(r);
            return Y(e, t, r), t;
        }, $t = (e, r, t, n, o)=>{
            var a = {
                string: (S)=>{
                    var I = 0;
                    return S != null && S !== 0 && (I = Zi(S)), I;
                },
                array: (S)=>{
                    var I = ar(S.length);
                    return Ji(S, I), I;
                }
            };
            function s(S) {
                return r === "string" ? X(S) : r === "boolean" ? !!S : S;
            }
            var l = Yi(e), d = [], _ = 0;
            if (c(r !== "array", 'Return type should not be "array".'), n) for(var v = 0; v < n.length; v++){
                var g = a[t[v]];
                g ? (_ === 0 && (_ = Xr()), d[v] = g(n[v])) : d[v] = n[v];
            }
            var m = l(...d);
            function p(S) {
                return _ !== 0 && or(_), s(S);
            }
            var k = o?.async;
            return k ? m.then(p) : (m = p(m), m);
        }, Qi = (e, r, t, n)=>(...o)=>$t(e, r, t, o, n), eo = (...e)=>Mt(...e);
        w.init(), i.createPreloadedFile = Wn, i.preloadFile = lt, i.staticInit(), c(q.length === 10);
        {
            if (un(), f.noExitRuntime && (cr = f.noExitRuntime), f.preloadPlugins && (st = f.preloadPlugins), f.print && (Q = f.print), f.printErr && (b = f.printErr), f.wasmBinary && (ke = f.wasmBinary), io(), f.arguments && f.arguments, f.thisProgram && (Je = f.thisProgram), c(typeof f.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), c(typeof f.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), c(typeof f.read > "u", "Module.read option was removed"), c(typeof f.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), c(typeof f.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), c(typeof f.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), c(typeof f.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), c(typeof f.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), c(typeof f.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), f.preInit) for(typeof f.preInit == "function" && (f.preInit = [
                f.preInit
            ]); f.preInit.length > 0;)f.preInit.shift()();
            We("preInit");
        }
        f.ccall = $t, f.cwrap = Qi, f.UTF8ToString = X, f.stringToUTF8 = Y, f.allocateUTF8 = eo;
        var ro = [
            "writeI53ToI64",
            "writeI53ToI64Clamped",
            "writeI53ToI64Signaling",
            "writeI53ToU64Clamped",
            "writeI53ToU64Signaling",
            "readI53FromI64",
            "readI53FromU64",
            "convertI32PairToI53",
            "convertI32PairToI53Checked",
            "convertU32PairToI53",
            "getTempRet0",
            "setTempRet0",
            "withStackSave",
            "inetPton4",
            "inetNtop4",
            "inetPton6",
            "inetNtop6",
            "readSockaddr",
            "writeSockaddr",
            "runMainThreadEmAsm",
            "jstoi_q",
            "autoResumeAudioContext",
            "getDynCaller",
            "dynCall",
            "asmjsMangle",
            "HandleAllocator",
            "addOnInit",
            "addOnPostCtor",
            "addOnPreMain",
            "addOnExit",
            "STACK_SIZE",
            "STACK_ALIGN",
            "POINTER_SIZE",
            "ASSERTIONS",
            "convertJsFunctionToWasm",
            "getEmptyTableSlot",
            "updateTableMap",
            "getFunctionAddress",
            "addFunction",
            "removeFunction",
            "intArrayToString",
            "stringToAscii",
            "registerKeyEventCallback",
            "maybeCStringToJsString",
            "findEventTarget",
            "getBoundingClientRect",
            "fillMouseEventData",
            "registerMouseEventCallback",
            "registerWheelEventCallback",
            "registerUiEventCallback",
            "registerFocusEventCallback",
            "fillDeviceOrientationEventData",
            "registerDeviceOrientationEventCallback",
            "fillDeviceMotionEventData",
            "registerDeviceMotionEventCallback",
            "screenOrientation",
            "fillOrientationChangeEventData",
            "registerOrientationChangeEventCallback",
            "fillFullscreenChangeEventData",
            "registerFullscreenChangeEventCallback",
            "JSEvents_requestFullscreen",
            "JSEvents_resizeCanvasForFullscreen",
            "registerRestoreOldStyle",
            "hideEverythingExceptGivenElement",
            "restoreHiddenElements",
            "setLetterbox",
            "softFullscreenResizeWebGLRenderTarget",
            "doRequestFullscreen",
            "fillPointerlockChangeEventData",
            "registerPointerlockChangeEventCallback",
            "registerPointerlockErrorEventCallback",
            "requestPointerLock",
            "fillVisibilityChangeEventData",
            "registerVisibilityChangeEventCallback",
            "registerTouchEventCallback",
            "fillGamepadEventData",
            "registerGamepadEventCallback",
            "registerBeforeUnloadEventCallback",
            "fillBatteryEventData",
            "registerBatteryEventCallback",
            "setCanvasElementSizeCallingThread",
            "setCanvasElementSizeMainThread",
            "setCanvasElementSize",
            "getCanvasSizeCallingThread",
            "getCanvasSizeMainThread",
            "getCanvasElementSize",
            "getCallstack",
            "convertPCtoSourceLocation",
            "wasiRightsToMuslOFlags",
            "wasiOFlagsToMuslOFlags",
            "safeSetTimeout",
            "setImmediateWrapped",
            "safeRequestAnimationFrame",
            "clearImmediateWrapped",
            "registerPostMainLoop",
            "registerPreMainLoop",
            "getPromise",
            "makePromise",
            "idsToPromises",
            "makePromiseCallback",
            "findMatchingCatch",
            "Browser_asyncPrepareDataCounter",
            "isLeapYear",
            "ydayFromDate",
            "arraySum",
            "addDays",
            "getSocketFromFD",
            "getSocketAddress",
            "FS_mkdirTree",
            "_setNetworkCallback",
            "heapObjectForWebGLType",
            "toTypedArrayIndex",
            "webgl_enable_ANGLE_instanced_arrays",
            "webgl_enable_OES_vertex_array_object",
            "webgl_enable_WEBGL_draw_buffers",
            "webgl_enable_WEBGL_multi_draw",
            "webgl_enable_EXT_polygon_offset_clamp",
            "webgl_enable_EXT_clip_control",
            "webgl_enable_WEBGL_polygon_mode",
            "emscriptenWebGLGet",
            "computeUnpackAlignedImageSize",
            "colorChannelsInGlTextureFormat",
            "emscriptenWebGLGetTexPixelData",
            "emscriptenWebGLGetUniform",
            "webglGetUniformLocation",
            "webglPrepareUniformLocationsBeforeFirstUse",
            "webglGetLeftBracePos",
            "emscriptenWebGLGetVertexAttrib",
            "__glGetActiveAttribOrUniform",
            "writeGLArray",
            "emscripten_webgl_destroy_context_before_on_calling_thread",
            "registerWebGlEventCallback",
            "ALLOC_NORMAL",
            "ALLOC_STACK",
            "allocate",
            "writeStringToMemory",
            "writeAsciiToMemory",
            "allocateUTF8OnStack",
            "demangle",
            "stackTrace",
            "getNativeTypeSize",
            "getFunctionArgsName",
            "requireRegisteredType",
            "createJsInvokerSignature",
            "PureVirtualError",
            "getBasestPointer",
            "registerInheritedInstance",
            "unregisterInheritedInstance",
            "getInheritedInstance",
            "getInheritedInstanceCount",
            "getLiveInheritedInstances",
            "enumReadValueFromPointer",
            "genericPointerToWireType",
            "constNoSmartPtrRawPointerToWireType",
            "nonConstNoSmartPtrRawPointerToWireType",
            "init_RegisteredPointer",
            "RegisteredPointer",
            "RegisteredPointer_fromWireType",
            "runDestructor",
            "releaseClassHandle",
            "detachFinalizer",
            "attachFinalizer",
            "makeClassHandle",
            "init_ClassHandle",
            "ClassHandle",
            "throwInstanceAlreadyDeleted",
            "flushPendingDeletes",
            "setDelayFunction",
            "RegisteredClass",
            "shallowCopyInternalPointer",
            "downcastPointer",
            "upcastPointer",
            "validateThis",
            "char_0",
            "char_9",
            "makeLegalFunctionName",
            "count_emval_handles",
            "getStringOrSymbol",
            "emval_returnValue",
            "emval_lookupTypes",
            "emval_addMethodCaller"
        ];
        ro.forEach(dn);
        var to = [
            "run",
            "out",
            "err",
            "callMain",
            "abort",
            "wasmExports",
            "HEAPF32",
            "HEAPF64",
            "HEAP8",
            "HEAP16",
            "HEAPU16",
            "HEAP32",
            "HEAPU32",
            "HEAP64",
            "HEAPU64",
            "writeStackCookie",
            "checkStackCookie",
            "INT53_MAX",
            "INT53_MIN",
            "bigintToI53Checked",
            "stackSave",
            "stackRestore",
            "stackAlloc",
            "createNamedFunction",
            "ptrToString",
            "zeroMemory",
            "exitJS",
            "getHeapMax",
            "growMemory",
            "ENV",
            "ERRNO_CODES",
            "strError",
            "DNS",
            "Protocols",
            "Sockets",
            "timers",
            "warnOnce",
            "readEmAsmArgsArray",
            "readEmAsmArgs",
            "runEmAsmFunction",
            "getExecutableName",
            "handleException",
            "keepRuntimeAlive",
            "runtimeKeepalivePush",
            "runtimeKeepalivePop",
            "callUserCallback",
            "maybeExit",
            "asyncLoad",
            "alignMemory",
            "mmapAlloc",
            "wasmTable",
            "wasmMemory",
            "getUniqueRunDependency",
            "noExitRuntime",
            "addRunDependency",
            "removeRunDependency",
            "addOnPreRun",
            "addOnPostRun",
            "freeTableIndexes",
            "functionsInTableMap",
            "setValue",
            "getValue",
            "PATH",
            "PATH_FS",
            "UTF8Decoder",
            "UTF8ArrayToString",
            "stringToUTF8Array",
            "lengthBytesUTF8",
            "intArrayFromString",
            "AsciiToString",
            "UTF16Decoder",
            "UTF16ToString",
            "stringToUTF16",
            "lengthBytesUTF16",
            "UTF32ToString",
            "stringToUTF32",
            "lengthBytesUTF32",
            "stringToNewUTF8",
            "stringToUTF8OnStack",
            "writeArrayToMemory",
            "JSEvents",
            "specialHTMLTargets",
            "findCanvasEventTarget",
            "currentFullscreenStrategy",
            "restoreOldWindowedStyle",
            "jsStackTrace",
            "UNWIND_CACHE",
            "ExitStatus",
            "getEnvStrings",
            "checkWasiClock",
            "doReadv",
            "doWritev",
            "initRandomFill",
            "randomFill",
            "emSetImmediate",
            "emClearImmediate_deps",
            "emClearImmediate",
            "promiseMap",
            "uncaughtExceptionCount",
            "exceptionLast",
            "exceptionCaught",
            "ExceptionInfo",
            "Browser",
            "requestFullscreen",
            "requestFullScreen",
            "setCanvasSize",
            "getUserMedia",
            "createContext",
            "getPreloadedImageData__data",
            "wget",
            "MONTH_DAYS_REGULAR",
            "MONTH_DAYS_LEAP",
            "MONTH_DAYS_REGULAR_CUMULATIVE",
            "MONTH_DAYS_LEAP_CUMULATIVE",
            "SYSCALLS",
            "preloadPlugins",
            "FS_createPreloadedFile",
            "FS_preloadFile",
            "FS_modeStringToFlags",
            "FS_getMode",
            "FS_stdin_getChar_buffer",
            "FS_stdin_getChar",
            "FS_unlink",
            "FS_createPath",
            "FS_createDevice",
            "FS_readFile",
            "FS",
            "FS_root",
            "FS_mounts",
            "FS_devices",
            "FS_streams",
            "FS_nextInode",
            "FS_nameTable",
            "FS_currentPath",
            "FS_initialized",
            "FS_ignorePermissions",
            "FS_filesystems",
            "FS_syncFSRequests",
            "FS_readFiles",
            "FS_lookupPath",
            "FS_getPath",
            "FS_hashName",
            "FS_hashAddNode",
            "FS_hashRemoveNode",
            "FS_lookupNode",
            "FS_createNode",
            "FS_destroyNode",
            "FS_isRoot",
            "FS_isMountpoint",
            "FS_isFile",
            "FS_isDir",
            "FS_isLink",
            "FS_isChrdev",
            "FS_isBlkdev",
            "FS_isFIFO",
            "FS_isSocket",
            "FS_flagsToPermissionString",
            "FS_nodePermissions",
            "FS_mayLookup",
            "FS_mayCreate",
            "FS_mayDelete",
            "FS_mayOpen",
            "FS_checkOpExists",
            "FS_nextfd",
            "FS_getStreamChecked",
            "FS_getStream",
            "FS_createStream",
            "FS_closeStream",
            "FS_dupStream",
            "FS_doSetAttr",
            "FS_chrdev_stream_ops",
            "FS_major",
            "FS_minor",
            "FS_makedev",
            "FS_registerDevice",
            "FS_getDevice",
            "FS_getMounts",
            "FS_syncfs",
            "FS_mount",
            "FS_unmount",
            "FS_lookup",
            "FS_mknod",
            "FS_statfs",
            "FS_statfsStream",
            "FS_statfsNode",
            "FS_create",
            "FS_mkdir",
            "FS_mkdev",
            "FS_symlink",
            "FS_rename",
            "FS_rmdir",
            "FS_readdir",
            "FS_readlink",
            "FS_stat",
            "FS_fstat",
            "FS_lstat",
            "FS_doChmod",
            "FS_chmod",
            "FS_lchmod",
            "FS_fchmod",
            "FS_doChown",
            "FS_chown",
            "FS_lchown",
            "FS_fchown",
            "FS_doTruncate",
            "FS_truncate",
            "FS_ftruncate",
            "FS_utime",
            "FS_open",
            "FS_close",
            "FS_isClosed",
            "FS_llseek",
            "FS_read",
            "FS_write",
            "FS_mmap",
            "FS_msync",
            "FS_ioctl",
            "FS_writeFile",
            "FS_cwd",
            "FS_chdir",
            "FS_createDefaultDirectories",
            "FS_createDefaultDevices",
            "FS_createSpecialDirectories",
            "FS_createStandardStreams",
            "FS_staticInit",
            "FS_init",
            "FS_quit",
            "FS_findObject",
            "FS_analyzePath",
            "FS_createFile",
            "FS_createDataFile",
            "FS_forceLoadFile",
            "FS_createLazyFile",
            "FS_absolutePath",
            "FS_createFolder",
            "FS_createLink",
            "FS_joinPath",
            "FS_mmapAlloc",
            "FS_standardizePath",
            "MEMFS",
            "TTY",
            "PIPEFS",
            "SOCKFS",
            "tempFixedLengthArray",
            "miniTempWebGLFloatBuffers",
            "miniTempWebGLIntBuffers",
            "GL",
            "AL",
            "GLUT",
            "EGL",
            "GLEW",
            "IDBStore",
            "runAndAbortIfError",
            "Asyncify",
            "Fibers",
            "SDL",
            "SDL_gfx",
            "print",
            "printErr",
            "jstoi_s",
            "PThread",
            "terminateWorker",
            "cleanupThread",
            "registerTLSInit",
            "spawnThread",
            "exitOnMainThread",
            "proxyToMainThread",
            "proxiedJSCallArgs",
            "invokeEntryPoint",
            "checkMailbox",
            "InternalError",
            "BindingError",
            "throwInternalError",
            "throwBindingError",
            "registeredTypes",
            "awaitingDependencies",
            "typeDependencies",
            "tupleRegistrations",
            "structRegistrations",
            "sharedRegisterType",
            "whenDependentTypesAreResolved",
            "getTypeName",
            "getFunctionName",
            "heap32VectorToArray",
            "usesDestructorStack",
            "checkArgCount",
            "getRequiredArgCount",
            "createJsInvoker",
            "UnboundTypeError",
            "EmValType",
            "EmValOptionalType",
            "throwUnboundTypeError",
            "ensureOverloadTable",
            "exposePublicSymbol",
            "replacePublicSymbol",
            "embindRepr",
            "registeredInstances",
            "registeredPointers",
            "registerType",
            "integerReadValueFromPointer",
            "floatReadValueFromPointer",
            "assertIntegerRange",
            "readPointer",
            "runDestructors",
            "craftInvokerFunction",
            "embind__requireFunction",
            "finalizationRegistry",
            "detachFinalizer_deps",
            "deletionQueue",
            "delayFunction",
            "emval_freelist",
            "emval_handles",
            "emval_symbols",
            "Emval",
            "emval_methodCallers"
        ];
        to.forEach(Ir);
        var no = [
            sr,
            qr,
            rt,
            dt,
            ct,
            ut,
            ft,
            _t,
            mt,
            vt,
            Ft,
            At,
            Nt,
            Ot,
            Wt,
            xt,
            Lt,
            Ut
        ];
        function io() {
            sn("fetchSettings");
        }
        var yr = {
            540644: ()=>typeof wasmOffsetConverter < "u"
        };
        function Bt() {
            return typeof wasmOffsetConverter < "u";
        }
        Bt.sig = "i";
        var jt = F("___getTypeName"), zt = F("__embind_initialize_bindings");
        f._get_cp_model_schema = F("_get_cp_model_schema"), f._get_sat_parameters_schema = F("_get_sat_parameters_schema"), f._solve_model = F("_solve_model");
        var Ke = f._malloc = F("_malloc");
        f._free_buffer = F("_free_buffer");
        var J = f._free = F("_free");
        f._interrupt_solve = F("_interrupt_solve"), f._validate_model = F("_validate_model");
        var ye = F("_pthread_self"), Er = F("_fflush"), Ht = F("_strerror"), Vt = F("_emscripten_builtin_memalign"), wr = F("__emscripten_thread_init"), Gt = F("__emscripten_thread_crashed"), br = F("_emscripten_stack_get_end"), Kt = F("__emscripten_run_js_on_main_thread"), Xt = F("__emscripten_thread_free_data"), Sr = F("__emscripten_thread_exit"), qt = F("__emscripten_check_mailbox"), Yt = F("_emscripten_stack_init"), Jt = F("_emscripten_stack_set_limits"), Zt = F("__emscripten_stack_restore"), Qt = F("__emscripten_stack_alloc"), kr = F("_emscripten_stack_get_current"), en = F("wasmTable");
        function oo(e) {
            c(typeof e.__getTypeName < "u", "missing Wasm export: __getTypeName"), c(typeof e._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), c(typeof e.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), c(typeof e.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), c(typeof e.solve_model < "u", "missing Wasm export: solve_model"), c(typeof e.malloc < "u", "missing Wasm export: malloc"), c(typeof e.free_buffer < "u", "missing Wasm export: free_buffer"), c(typeof e.free < "u", "missing Wasm export: free"), c(typeof e.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), c(typeof e.validate_model < "u", "missing Wasm export: validate_model"), c(typeof e.pthread_self < "u", "missing Wasm export: pthread_self"), c(typeof e.fflush < "u", "missing Wasm export: fflush"), c(typeof e.strerror < "u", "missing Wasm export: strerror"), c(typeof e._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), c(typeof e.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), c(typeof e._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), c(typeof e._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), c(typeof e.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), c(typeof e.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), c(typeof e._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), c(typeof e._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), c(typeof e._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), c(typeof e._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), c(typeof e.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), c(typeof e.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), c(typeof e.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), c(typeof e._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), c(typeof e._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), c(typeof e.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), c(typeof e.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), jt = D("__getTypeName", 1), zt = D("_embind_initialize_bindings", 0), f._get_cp_model_schema = D("get_cp_model_schema", 0), f._get_sat_parameters_schema = D("get_sat_parameters_schema", 0), f._solve_model = D("solve_model", 5), Ke = f._malloc = D("malloc", 1), f._free_buffer = D("free_buffer", 1), J = f._free = D("free", 1), f._interrupt_solve = D("interrupt_solve", 0), f._validate_model = D("validate_model", 3), ye = D("pthread_self", 0), Er = D("fflush", 1), Ht = D("strerror", 1), Vt = D("emscripten_builtin_memalign", 2), wr = D("_emscripten_thread_init", 6), Gt = D("_emscripten_thread_crashed", 0), br = e.emscripten_stack_get_end, e.emscripten_stack_get_base, Kt = D("_emscripten_run_js_on_main_thread", 5), Xt = D("_emscripten_thread_free_data", 1), Sr = D("_emscripten_thread_exit", 1), qt = D("_emscripten_check_mailbox", 0), Yt = e.emscripten_stack_init, Jt = e.emscripten_stack_set_limits, e.emscripten_stack_get_free, Zt = e._emscripten_stack_restore, Qt = e._emscripten_stack_alloc, kr = e.emscripten_stack_get_current, en = e.__indirect_function_table;
        }
        var Ee;
        function ao() {
            Ee = {
                HaveOffsetConverter: Bt,
                __assert_fail: Sn,
                __cxa_throw: Tn,
                __pthread_create_js: tt,
                __syscall_fcntl64: dt,
                __syscall_fstat64: ct,
                __syscall_ioctl: ut,
                __syscall_lstat64: ft,
                __syscall_newfstatat: _t,
                __syscall_openat: mt,
                __syscall_stat64: vt,
                _abort_js: xn,
                _embind_register_bigint: $n,
                _embind_register_bool: Bn,
                _embind_register_emval: Hn,
                _embind_register_float: Gn,
                _embind_register_function: di,
                _embind_register_integer: ci,
                _embind_register_memory_view: ui,
                _embind_register_std_string: fi,
                _embind_register_std_wstring: yi,
                _embind_register_void: Ei,
                _emscripten_init_main_thread_js: wi,
                _emscripten_notify_mailbox_postmessage: Si,
                _emscripten_receive_on_main_thread_js: ki,
                _emscripten_thread_cleanup: Ti,
                _emscripten_thread_mailbox_await: hr,
                _emscripten_thread_set_strongref: Fi,
                _mmap_js: Ft,
                _munmap_js: At,
                _tzset_js: Ci,
                clock_time_get: Di,
                emscripten_asm_const_int: Oi,
                emscripten_check_blocking_allowed: Wi,
                emscripten_errn: xi,
                emscripten_exit_with_live_runtime: Li,
                emscripten_get_heap_max: Ui,
                emscripten_get_now: Pt,
                emscripten_num_logical_cores: $i,
                emscripten_pc_get_function: Ge,
                emscripten_resize_heap: zi,
                emscripten_stack_snapshot: Bi,
                emscripten_stack_unwind_buffer: Hi,
                environ_get: Nt,
                environ_sizes_get: Ot,
                exit: lr,
                fd_close: Wt,
                fd_read: xt,
                fd_seek: Lt,
                fd_write: Ut,
                memory: z,
                proc_exit: sr,
                random_get: Xi
            };
        }
        var rn;
        function so() {
            c(!y), Yt(), Cr();
        }
        function Xe() {
            if (oe > 0) {
                Pe = Xe;
                return;
            }
            if (y) {
                rr?.(f), Wr();
                return;
            }
            if (so(), fn(), oe > 0) {
                Pe = Xe;
                return;
            }
            async function e() {
                c(!rn), rn = !0, f.calledRun = !0, !de && (Wr(), rr?.(f), f.onRuntimeInitialized?.(), We("onRuntimeInitialized"), c(!f._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), _n());
            }
            f.setStatus ? (f.setStatus("Running..."), setTimeout(()=>{
                setTimeout(()=>f.setStatus(""), 1), e();
            }, 1)) : e(), Te();
        }
        function lo() {
            var e = Q, r = b, t = !1;
            Q = b = (d)=>{
                t = !0;
            };
            try {
                Er(0);
                for (var n of [
                    "stdout",
                    "stderr"
                ]){
                    var o = i.analyzePath("/dev/" + n);
                    if (!o) return;
                    var a = o.object, s = a.rdev, l = te.ttys[s];
                    l?.output?.length && (t = !0);
                }
            } catch  {}
            Q = e, b = r, t && re("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
        }
        var Z;
        y || (Z = await Lr(), Xe()), ue ? Ye = f : Ye = new Promise((e, r)=>{
            rr = e, tr = r;
        });
        for (const e of Object.keys(f))e in qe || Object.defineProperty(qe, e, {
            configurable: !0,
            get () {
                A(`Access to module property ('${e}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
            }
        });
        return Ye;
    }
    var nn = globalThis.self?.name?.startsWith("em-pthread"), uo = globalThis.process?.versions?.node && globalThis.process?.type != "renderer";
    uo && (nn = (await Promise.resolve().then(function() {
        return tn;
    })).workerData === "em-pthread");
    nn && co();
})();
