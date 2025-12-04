(async ()=>{
    var an = Object.freeze({
        __proto__: null
    });
    async function sn(M = {}) {
        var P;
        (function() {
            function e(d) {
                d = d.split("-")[0];
                for(var m = d.split(".").slice(0, 3); m.length < 3;)m.push("00");
                return m = m.map((v, g, _)=>v.padStart(2, "0")), m.join("");
            }
            var r = (d)=>[
                    d / 1e4 | 0,
                    (d / 100 | 0) % 100,
                    d % 100
                ].join("."), t = 2147483647, n = typeof process < "u" && process.versions?.node ? e(process.versions.node) : t;
            if (n < 160400) throw new Error(`This emscripten-generated code requires node v${r(160400)} (detected v${r(n)})`);
            var i = typeof navigator < "u" && navigator.userAgent;
            if (i) {
                var a = i.includes("Safari/") && i.match(/Version\/(\d+\.?\d*\.?\d*)/) ? e(i.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : t;
                if (a < 15e4) throw new Error(`This emscripten-generated code requires Safari v${r(15e4)} (detected v${a})`);
                var s = i.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(i.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : t;
                if (s < 114) throw new Error(`This emscripten-generated code requires Firefox v114 (detected v${s})`);
                var l = i.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(i.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : t;
                if (l < 85) throw new Error(`This emscripten-generated code requires Chrome v85 (detected v${l})`);
            }
        })();
        var c = M, B = !!globalThis.window, L = !!globalThis.WorkerGlobalScope, T = globalThis.process?.versions?.node && globalThis.process?.type != "renderer", me = !B && !T && !L, y = L && self.name?.startsWith("em-pthread");
        if (y && (u(!globalThis.moduleLoaded, "module should only be loaded once on each pthread worker"), globalThis.moduleLoaded = !0), T) {
            const { createRequire: e } = await Promise.resolve().then(function() {
                return an;
            });
            var J = e(import.meta.url), xe = J("worker_threads");
            global.Worker = xe.Worker, L = !xe.isMainThread, y = L && xe.workerData == "em-pthread";
        }
        var er = "./this.program", rr = (e, r)=>{
            throw r;
        }, tr = import.meta.url, Le = "";
        function cn(e) {
            return c.locateFile ? c.locateFile(e, Le) : Le + e;
        }
        var Ue, Pe;
        if (T) {
            if (!(globalThis.process?.versions?.node && globalThis.process?.type != "renderer")) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
            var Ce = J("fs");
            tr.startsWith("file:") && (Le = J("path").dirname(J("url").fileURLToPath(tr)) + "/"), Pe = (r)=>{
                r = $e(r) ? new URL(r) : r;
                var t = Ce.readFileSync(r);
                return u(Buffer.isBuffer(t)), t;
            }, Ue = async (r, t = !0)=>{
                r = $e(r) ? new URL(r) : r;
                var n = Ce.readFileSync(r, t ? void 0 : "utf8");
                return u(t ? Buffer.isBuffer(n) : typeof n == "string"), n;
            }, process.argv.length > 1 && (er = process.argv[1].replace(/\\/g, "/")), process.argv.slice(2), rr = (r, t)=>{
                throw process.exitCode = r, t;
            };
        } else if (!me) if (B || L) {
            try {
                Le = new URL(".", tr).href;
            } catch  {}
            if (!(globalThis.window || globalThis.WorkerGlobalScope)) throw new Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
            T || (L && (Pe = (e)=>{
                var r = new XMLHttpRequest();
                return r.open("GET", e, !1), r.responseType = "arraybuffer", r.send(null), new Uint8Array(r.response);
            }), Ue = async (e)=>{
                u(!$e(e), "readAsync does not work with file:// URLs");
                var r = await fetch(e, {
                    credentials: "same-origin"
                });
                if (r.ok) return r.arrayBuffer();
                throw new Error(r.status + " : " + r.url);
            });
        } else throw new Error("environment detection error");
        var Cr = console.log.bind(console), Ir = console.error.bind(console);
        if (T) {
            var un = J("util"), Mr = (e)=>typeof e == "object" ? un.inspect(e) : e;
            Cr = (...e)=>Ce.writeSync(1, e.map(Mr).join(" ") + `
`), Ir = (...e)=>Ce.writeSync(2, e.map(Mr).join(" ") + `
`);
        }
        var ne = Cr, b = Ir;
        u(B || L || T, "Pthreads do not work in this environment yet (need Web Workers, or an alternative to them)"), u(!me, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
        var Ie;
        globalThis.WebAssembly || b("no native wasm support detected");
        var se, _e = !1, ve;
        function u(e, r) {
            e || C("Assertion failed" + (r ? ": " + r : ""));
        }
        var $e = (e)=>e.startsWith("file://");
        function Dr() {
            var e = Tr();
            u((e & 3) == 0), e == 0 && (e += 4), (f(), h)[e >> 2] = 34821223, (f(), h)[e + 4 >> 2] = 2310721022, (f(), h)[0] = 1668509029;
        }
        function Me() {
            if (!_e) {
                var e = Tr();
                e == 0 && (e += 4);
                var r = (f(), h)[e >> 2], t = (f(), h)[e + 4 >> 2];
                (r != 34821223 || t != 2310721022) && C(`Stack overflow! Stack cookie has been overwritten at ${ce(e)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${ce(t)} ${ce(r)}`), (f(), h)[0] != 1668509029 && C("Runtime error: The application has corrupted its heap memory area (address zero)!");
            }
        }
        function nr(...e) {
            if (T) {
                let n = function(i) {
                    switch(typeof i){
                        case "object":
                            return t.inspect(i);
                        case "undefined":
                            return "undefined";
                    }
                    return i;
                };
                var r = J("fs"), t = J("util");
                r.writeSync(2, e.map(n).join(" ") + `
`);
            } else console.warn(...e);
        }
        (()=>{
            var e = new Int16Array(1), r = new Int8Array(e.buffer);
            e[0] = 25459, (r[0] !== 115 || r[1] !== 99) && C("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
        })();
        function Be(e) {
            Object.getOwnPropertyDescriptor(c, e) || Object.defineProperty(c, e, {
                configurable: !0,
                set () {
                    C(`Attempt to set \`Module.${e}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
                }
            });
        }
        function A(e) {
            return ()=>u(!1, `call to '${e}' via reference taken before Wasm module initialization`);
        }
        function fn(e) {
            Object.getOwnPropertyDescriptor(c, e) && C(`\`Module.${e}\` was supplied but \`${e}\` not included in INCOMING_MODULE_JS_API`);
        }
        function mn(e) {
            return e === "FS_createPath" || e === "FS_createDataFile" || e === "FS_createPreloadedFile" || e === "FS_preloadFile" || e === "FS_unlink" || e === "addRunDependency" || e === "FS_createLazyFile" || e === "FS_createDevice" || e === "removeRunDependency";
        }
        function _n(e) {
            Rr(e);
        }
        function Rr(e) {
            y || Object.getOwnPropertyDescriptor(c, e) || Object.defineProperty(c, e, {
                configurable: !0,
                get () {
                    var r = `'${e}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
                    mn(e) && (r += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you"), C(r);
                }
            });
        }
        function vn() {
            function e() {
                var t = 0;
                return he && typeof ke < "u" && (t = ke()), `w:${Or},t:${ce(t)}:`;
            }
            var r = nr;
            nr = (...t)=>r(e(), ...t);
        }
        vn();
        function f() {
            K.buffer != U.buffer && je();
        }
        var or, ir;
        if (T && y) {
            var Nr = xe.parentPort;
            Nr.on("message", (e)=>global.onmessage?.({
                    data: e
                })), Object.assign(globalThis, {
                self: global,
                postMessage: (e)=>Nr.postMessage(e)
            }), process.on("uncaughtException", (e)=>{
                postMessage({
                    cmd: "uncaughtException",
                    error: e
                }), process.exit(1);
            });
        }
        var Or = 0, Wr;
        if (y) {
            var ar = !1;
            self.onunhandledrejection = (r)=>{
                throw r.reason || r;
            };
            async function e(r) {
                try {
                    var t = r.data, n = t.cmd;
                    if (n === "load") {
                        Or = t.workerID;
                        let i = [];
                        self.onmessage = (a)=>i.push(a), Wr = ()=>{
                            postMessage({
                                cmd: "loaded"
                            });
                            for (let a of i)e(a);
                            self.onmessage = e;
                        };
                        for (const a of t.handlers)(!c[a] || c[a].proxy) && (c[a] = (...s)=>{
                            postMessage({
                                cmd: "callHandler",
                                handler: a,
                                args: s
                            });
                        }, a == "print" && (ne = c[a]), a == "printErr" && (b = c[a]));
                        K = t.wasmMemory, je(), se = t.wasmModule, Br(), Qe();
                    } else if (n === "run") {
                        u(t.pthread_ptr), Fn(t.pthread_ptr), kr(t.pthread_ptr, 0, 0, 1, 0, 0), w.threadInitTLS(), yr(t.pthread_ptr), ar || (Gt(), ar = !0);
                        try {
                            await rt(t.start_routine, t.arg);
                        } catch (i) {
                            if (i != "unwind") throw i;
                        }
                    } else t.target === "setimmediate" || (n === "checkMailbox" ? ar && Ke() : n && (b(`worker: received unknown command ${n}`), b(t)));
                } catch (i) {
                    throw b(`worker: onmessage() captured an uncaught exception: ${i}`), i?.stack && b(i.stack), qt(), i;
                }
            }
            self.onmessage = e;
        }
        var U, H, le, De, R, h, xr, Re, O, Lr, he = !1;
        function je() {
            var e = K.buffer;
            U = new Int8Array(e), le = new Int16Array(e), c.HEAPU8 = H = new Uint8Array(e), De = new Uint16Array(e), R = new Int32Array(e), h = new Uint32Array(e), xr = new Float32Array(e), Re = new Float64Array(e), O = new BigInt64Array(e), Lr = new BigUint64Array(e);
        }
        function hn() {
            if (!y) {
                if (c.wasmMemory) K = c.wasmMemory;
                else {
                    var e = c.INITIAL_MEMORY || 16777216;
                    u(e >= 65536, "INITIAL_MEMORY should be larger than STACK_SIZE, was " + e + "! (STACK_SIZE=65536)"), K = new WebAssembly.Memory({
                        initial: e / 65536,
                        maximum: 32768,
                        shared: !0
                    });
                }
                je();
            }
        }
        u(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
        function pn() {
            if (u(!y), c.preRun) for(typeof c.preRun == "function" && (c.preRun = [
                c.preRun
            ]); c.preRun.length;)Kr(c.preRun.shift());
            Be("preRun"), Vr(Gr);
        }
        function Ur() {
            if (u(!he), he = !0, y) return Wr();
            Me(), !c.noFSInit && !o.initialized && o.init(), te.__wasm_call_ctors(), o.ignorePermissions = !1;
        }
        function gn() {
            if (Me(), !y) {
                if (c.postRun) for(typeof c.postRun == "function" && (c.postRun = [
                    c.postRun
                ]); c.postRun.length;)Tn(c.postRun.shift());
                Be("postRun"), Vr(Qr);
            }
        }
        function C(e) {
            c.onAbort?.(e), e = "Aborted(" + e + ")", b(e), _e = !0;
            var r = new WebAssembly.RuntimeError(e);
            throw ir?.(r), r;
        }
        function W(e, r) {
            return (...t)=>{
                u(he, `native function \`${e}\` called before runtime initialization`);
                var n = te[e];
                return u(n, `exported native function \`${e}\` not found`), u(t.length <= r, `native function \`${e}\` called with ${t.length} args but expects ${r}`), n(...t);
            };
        }
        var sr;
        function yn() {
            return c.locateFile ? cn("cp_sat_runtime.wasm") : new URL("/assets/cp_sat_runtime-C04Aywpi.wasm", import.meta.url).href;
        }
        function En(e) {
            if (e == sr && Ie) return new Uint8Array(Ie);
            if (Pe) return Pe(e);
            throw "both async and sync fetching of the wasm failed";
        }
        async function wn(e) {
            if (!Ie) try {
                var r = await Ue(e);
                return new Uint8Array(r);
            } catch  {}
            return En(e);
        }
        async function bn(e, r) {
            try {
                var t = await wn(e), n = await WebAssembly.instantiate(t, r);
                return n;
            } catch (i) {
                b(`failed to asynchronously prepare wasm: ${i}`), $e(e) && b(`warning: Loading from a file URI (${e}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), C(i);
            }
        }
        async function Sn(e, r, t) {
            if (!e && !T) try {
                var n = fetch(r, {
                    credentials: "same-origin"
                }), i = await WebAssembly.instantiateStreaming(n, t);
                return i;
            } catch (a) {
                b(`wasm streaming compile failed: ${a}`), b("falling back to ArrayBuffer instantiation");
            }
            return bn(r, t);
        }
        function $r() {
            fi(), Te.__instrumented || (Te.__instrumented = !0, q.instrumentWasmImports(Te));
            var e = {
                env: Te,
                wasi_snapshot_preview1: Te
            };
            return e;
        }
        async function Br() {
            function e(l, d) {
                return te = l.exports, te = q.instrumentWasmExports(te), An(te._emscripten_tls_init), ui(te), se = d, te;
            }
            var r = c;
            function t(l) {
                return u(c === r, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?"), r = null, e(l.instance, l.module);
            }
            var n = $r();
            if (c.instantiateWasm) return new Promise((l, d)=>{
                try {
                    c.instantiateWasm(n, (m, v)=>{
                        l(e(m, v));
                    });
                } catch (m) {
                    b(`Module.instantiateWasm callback failed with error: ${m}`), d(m);
                }
            });
            if (y) {
                u(se, "wasmModule should have been received via postMessage");
                var i = new WebAssembly.Instance(se, $r());
                return e(i, se);
            }
            sr ??= yn();
            var a = await Sn(Ie, sr, n), s = t(a);
            return s;
        }
        class jr {
            name = "ExitStatus";
            constructor(r){
                this.message = `Program terminated with exit(${r})`, this.status = r;
            }
        }
        var zr = (e)=>{
            e.terminate(), e.onmessage = (r)=>{
                var t = r.data.cmd;
                b(`received "${t}" command from terminated worker: ${e.workerID}`);
            };
        }, Hr = (e)=>{
            u(!y, "Internal Error! cleanupThread() can only ever be called from main application thread!"), u(e, "Internal Error! Null pthread_ptr in cleanupThread!");
            var r = w.pthreads[e];
            u(r), w.returnWorkerToPool(r);
        }, Vr = (e)=>{
            for(; e.length > 0;)e.shift()(c);
        }, Gr = [], Kr = (e)=>Gr.push(e), de = 0, Ne = null, pe = {}, oe = null, Xr = (e)=>{
            if (de--, c.monitorRunDependencies?.(de), u(e, "removeRunDependency requires an ID"), u(pe[e]), delete pe[e], de == 0 && (oe !== null && (clearInterval(oe), oe = null), Ne)) {
                var r = Ne;
                Ne = null, r();
            }
        }, qr = (e)=>{
            de++, c.monitorRunDependencies?.(de), u(e, "addRunDependency requires an ID"), u(!pe[e]), pe[e] = 1, oe === null && globalThis.setInterval && (oe = setInterval(()=>{
                if (_e) {
                    clearInterval(oe), oe = null;
                    return;
                }
                var r = !1;
                for(var t in pe)r || (r = !0, b("still waiting on run dependencies:")), b(`dependency: ${t}`);
                r && b("(end of list)");
            }, 1e4), oe.unref?.());
        }, Yr = (e)=>{
            u(!y, "Internal Error! spawnThread() can only ever be called from main application thread!"), u(e.pthread_ptr, "Internal error, no pthread ptr!");
            var r = w.getNewWorker();
            if (!r) return 6;
            u(!r.pthread_ptr, "Internal error!"), w.runningWorkers.push(r), w.pthreads[e.pthread_ptr] = r, r.pthread_ptr = e.pthread_ptr;
            var t = {
                cmd: "run",
                start_routine: e.startRoutine,
                arg: e.arg,
                pthread_ptr: e.pthread_ptr
            };
            return T && r.unref(), r.postMessage(t, e.transferList), 0;
        }, ge = 0, ze = ()=>mr || ge > 0, Jr = ()=>Ar(), lr = (e)=>rn(e), dr = (e)=>tn(e), x = (e, r, t, ...n)=>{
            for(var i = n.length * 2, a = Jr(), s = dr(i * 8), l = s >> 3, d = 0; d < n.length; d++){
                var m = n[d];
                typeof m == "bigint" ? ((f(), O)[l + 2 * d] = 1n, (f(), O)[l + 2 * d + 1] = m) : ((f(), O)[l + 2 * d] = 0n, (f(), Re)[l + 2 * d + 1] = m);
            }
            var v = Yt(e, r, i, s, t);
            return lr(a), v;
        };
        function cr(e) {
            if (y) return x(0, 0, 1, e);
            ve = e, ze() || (w.terminateAllThreads(), c.onExit?.(e), _e = !0), rr(e, new jr(e));
        }
        function Zr(e) {
            if (y) return x(1, 0, 0, e);
            ur(e);
        }
        var kn = (e, r)=>{
            if (ve = e, _i(), y) throw u(!r), Zr(e), "unwind";
            if (ze() && !r) {
                var t = `program exited (with status: ${e}), but keepRuntimeAlive() is set (counter=${ge}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`;
                ir?.(t), b(t);
            }
            cr(e);
        }, ur = kn, ce = (e)=>(u(typeof e == "number", `ptrToString expects a number, got ${typeof e}`), e >>>= 0, "0x" + e.toString(16).padStart(8, "0")), w = {
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
                Kr(async ()=>{
                    var r = w.loadWasmModuleToAllWorkers();
                    qr("loading-workers"), await r, Xr("loading-workers");
                });
            },
            terminateAllThreads: ()=>{
                u(!y, "Internal Error! terminateAllThreads() can only ever be called from main application thread!");
                for (var e of w.runningWorkers)zr(e);
                for (var e of w.unusedWorkers)zr(e);
                w.unusedWorkers = [], w.runningWorkers = [], w.pthreads = {};
            },
            returnWorkerToPool: (e)=>{
                var r = e.pthread_ptr;
                delete w.pthreads[r], w.unusedWorkers.push(e), w.runningWorkers.splice(w.runningWorkers.indexOf(e), 1), e.pthread_ptr = 0, Jt(r);
            },
            threadInitTLS () {
                w.tlsInitFunctions.forEach((e)=>e());
            },
            loadWasmModuleToWorker: (e)=>new Promise((r)=>{
                    e.onmessage = (a)=>{
                        var s = a.data, l = s.cmd;
                        if (s.targetThread && s.targetThread != ke()) {
                            var d = w.pthreads[s.targetThread];
                            d ? d.postMessage(s, s.transferList) : b(`Internal error! Worker sent a message "${l}" to target pthread ${s.targetThread}, but that thread no longer exists!`);
                            return;
                        }
                        l === "checkMailbox" ? Ke() : l === "spawnThread" ? Yr(s) : l === "cleanupThread" ? Pt(()=>Hr(s.thread)) : l === "loaded" ? (e.loaded = !0, T && !e.pthread_ptr && e.unref(), r(e)) : s.target === "setimmediate" ? e.postMessage(s) : l === "uncaughtException" ? e.onerror(s.error) : l === "callHandler" ? c[s.handler](...s.args) : l && b(`worker sent an unknown command ${l}`);
                    }, e.onerror = (a)=>{
                        var s = "worker sent an error!";
                        throw e.pthread_ptr && (s = `Pthread ${ce(e.pthread_ptr)} sent an error!`), b(`${s} ${a.filename}:${a.lineno}: ${a.message}`), a;
                    }, T && (e.on("message", (a)=>e.onmessage({
                            data: a
                        })), e.on("error", (a)=>e.onerror(a))), u(K instanceof WebAssembly.Memory, "WebAssembly memory should have been loaded by now!"), u(se instanceof WebAssembly.Module, "WebAssembly Module should have been loaded by now!");
                    var t = [], n = [
                        "onExit",
                        "onAbort",
                        "print",
                        "printErr"
                    ];
                    for (var i of n)c.propertyIsEnumerable(i) && t.push(i);
                    e.postMessage({
                        cmd: "load",
                        handlers: t,
                        wasmMemory: K,
                        wasmModule: se,
                        workerID: e.workerID
                    });
                }),
            async loadWasmModuleToAllWorkers () {
                return y ? void 0 : Promise.all(w.unusedWorkers.map(w.loadWasmModuleToWorker));
            },
            allocateUnusedWorker () {
                var e;
                if (c.mainScriptUrlOrBlob) {
                    var r = c.mainScriptUrlOrBlob;
                    typeof r != "string" && (r = URL.createObjectURL(r)), e = new Worker(r, {
                        type: "module",
                        workerData: "em-pthread",
                        name: "em-pthread-" + w.nextWorkerID
                    });
                } else e = new Worker(new URL("/assets/cp_sat_runtime-DcNmBbPM.js", import.meta.url), {
                    type: "module",
                    workerData: "em-pthread",
                    name: "em-pthread-" + w.nextWorkerID
                });
                e.workerID = w.nextWorkerID++, w.unusedWorkers.push(e);
            },
            getNewWorker () {
                return w.unusedWorkers.length == 0 && (T || b("Tried to spawn a new thread, but the thread pool is exhausted.\nThis might result in a deadlock unless some threads eventually exit or the code explicitly breaks out to the event loop.\nIf you want to increase the pool size, use setting `-sPTHREAD_POOL_SIZE=...`.\nIf you want to throw an explicit error instead of the risk of deadlocking in those cases, use setting `-sPTHREAD_POOL_SIZE_STRICT=2`."), w.allocateUnusedWorker(), w.loadWasmModuleToWorker(w.unusedWorkers[0])), w.unusedWorkers.pop();
            }
        }, Qr = [], Tn = (e)=>Qr.push(e);
        function Fn(e) {
            var r = (f(), h)[e + 52 >> 2], t = (f(), h)[e + 56 >> 2], n = r - t;
            u(r != 0), u(n != 0), u(r > n, "stackHigh must be higher then stackLow"), en(r, n), lr(r), Dr();
        }
        var fr = [], et = (e)=>{
            var r = fr[e];
            return r || (fr[e] = r = nn.get(e), q.isAsyncExport(r) && (fr[e] = r = q.makeAsyncFunction(r))), r;
        }, rt = async (e, r)=>{
            ge = 0, mr = 0;
            var t = WebAssembly.promising(et(e))(r);
            Me();
            function n(i) {
                if (ze()) {
                    ve = i;
                    return;
                }
                Fr(i);
            }
            t = await t, n(t);
        };
        rt.isAsync = !0;
        var mr = !0, An = (e)=>w.tlsInitFunctions.push(e), ie = (e)=>{
            ie.shown ||= {}, ie.shown[e] || (ie.shown[e] = 1, T && (e = "warning: " + e), b(e));
        }, K, tt = globalThis.TextDecoder && new TextDecoder(), nt = (e, r, t, n)=>{
            var i = r + t;
            if (n) return i;
            for(; e[r] && !(r >= i);)++r;
            return r;
        }, ye = (e, r = 0, t, n)=>{
            var i = nt(e, r, t, n);
            if (i - r > 16 && e.buffer && tt) return tt.decode(e.buffer instanceof ArrayBuffer ? e.subarray(r, i) : e.slice(r, i));
            for(var a = ""; r < i;){
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
                if ((s & 240) == 224 ? s = (s & 15) << 12 | l << 6 | d : ((s & 248) != 240 && ie("Invalid UTF-8 leading byte " + ce(s) + " encountered when deserializing a UTF-8 string in wasm memory to a JS string!"), s = (s & 7) << 18 | l << 12 | d << 6 | e[r++] & 63), s < 65536) a += String.fromCharCode(s);
                else {
                    var m = s - 65536;
                    a += String.fromCharCode(55296 | m >> 10, 56320 | m & 1023);
                }
            }
            return a;
        }, Z = (e, r, t)=>(u(typeof e == "number", `UTF8ToString expects a number (got ${typeof e})`), e ? ye((f(), H), e, r, t) : ""), Pn = (e, r, t, n)=>C(`Assertion failed: ${Z(e)}, at: ` + [
                r ? Z(r) : "unknown filename",
                t,
                n ? Z(n) : "unknown function"
            ]);
        class Cn {
            constructor(r){
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
                r = r ? 1 : 0, (f(), U)[this.ptr + 12] = r;
            }
            get_caught() {
                return (f(), U)[this.ptr + 12] != 0;
            }
            set_rethrown(r) {
                r = r ? 1 : 0, (f(), U)[this.ptr + 13] = r;
            }
            get_rethrown() {
                return (f(), U)[this.ptr + 13] != 0;
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
        var In = (e, r, t)=>{
            var n = new Cn(e);
            n.init(r, t), u(!1, "Exception thrown, but exception catching is not enabled. Compile with -sNO_DISABLE_EXCEPTION_CATCHING or -sEXCEPTION_CATCHING_ALLOWED=[..] to catch.");
        };
        function ot(e, r, t, n) {
            return y ? x(2, 0, 1, e, r, t, n) : it(e, r, t, n);
        }
        var Mn = ()=>!!globalThis.SharedArrayBuffer, it = (e, r, t, n)=>{
            if (!Mn()) return nr("pthread_create: environment does not support SharedArrayBuffer, pthreads are not available"), 6;
            var i = [], a = 0;
            if (y && (i.length === 0 || a)) return ot(e, r, t, n);
            var s = {
                startRoutine: t,
                pthread_ptr: e,
                arg: n,
                transferList: i
            };
            return y ? (s.cmd = "spawnThread", postMessage(s, i), 0) : Yr(s);
        }, He = ()=>{
            u(I.varargs != null);
            var e = (f(), R)[+I.varargs >> 2];
            return I.varargs += 4, e;
        }, Ee = He, F = {
            isAbs: (e)=>e.charAt(0) === "/",
            splitPath: (e)=>{
                var r = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
                return r.exec(e).slice(1);
            },
            normalizeArray: (e, r)=>{
                for(var t = 0, n = e.length - 1; n >= 0; n--){
                    var i = e[n];
                    i === "." ? e.splice(n, 1) : i === ".." ? (e.splice(n, 1), t++) : t && (e.splice(n, 1), t--);
                }
                if (r) for(; t; t--)e.unshift("..");
                return e;
            },
            normalize: (e)=>{
                var r = F.isAbs(e), t = e.slice(-1) === "/";
                return e = F.normalizeArray(e.split("/").filter((n)=>!!n), !r).join("/"), !e && !r && (e = "."), e && t && (e += "/"), (r ? "/" : "") + e;
            },
            dirname: (e)=>{
                var r = F.splitPath(e), t = r[0], n = r[1];
                return !t && !n ? "." : (n && (n = n.slice(0, -1)), t + n);
            },
            basename: (e)=>e && e.match(/([^\/]+|\/)\/*$/)[1],
            join: (...e)=>F.normalize(e.join("/")),
            join2: (e, r)=>F.normalize(e + "/" + r)
        }, Dn = ()=>{
            if (T) {
                var e = J("crypto");
                return (r)=>e.randomFillSync(r);
            }
            return (r)=>r.set(crypto.getRandomValues(new Uint8Array(r.byteLength)));
        }, _r = (e)=>{
            (_r = Dn())(e);
        }, we = {
            resolve: (...e)=>{
                for(var r = "", t = !1, n = e.length - 1; n >= -1 && !t; n--){
                    var i = n >= 0 ? e[n] : o.cwd();
                    if (typeof i != "string") throw new TypeError("Arguments to path.resolve must be strings");
                    if (!i) return "";
                    r = i + "/" + r, t = F.isAbs(i);
                }
                return r = F.normalizeArray(r.split("/").filter((a)=>!!a), !t).join("/"), (t ? "/" : "") + r || ".";
            },
            relative: (e, r)=>{
                e = we.resolve(e).slice(1), r = we.resolve(r).slice(1);
                function t(m) {
                    for(var v = 0; v < m.length && m[v] === ""; v++);
                    for(var g = m.length - 1; g >= 0 && m[g] === ""; g--);
                    return v > g ? [] : m.slice(v, g - v + 1);
                }
                for(var n = t(e.split("/")), i = t(r.split("/")), a = Math.min(n.length, i.length), s = a, l = 0; l < a; l++)if (n[l] !== i[l]) {
                    s = l;
                    break;
                }
                for(var d = [], l = s; l < n.length; l++)d.push("..");
                return d = d.concat(i.slice(s)), d.join("/");
            }
        }, vr = [], ue = (e)=>{
            for(var r = 0, t = 0; t < e.length; ++t){
                var n = e.charCodeAt(t);
                n <= 127 ? r++ : n <= 2047 ? r += 2 : n >= 55296 && n <= 57343 ? (r += 4, ++t) : r += 3;
            }
            return r;
        }, at = (e, r, t, n)=>{
            if (u(typeof e == "string", `stringToUTF8Array expects a string (got ${typeof e})`), !(n > 0)) return 0;
            for(var i = t, a = t + n - 1, s = 0; s < e.length; ++s){
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
                    l > 1114111 && ie("Invalid Unicode code point " + ce(l) + " encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF)."), r[t++] = 240 | l >> 18, r[t++] = 128 | l >> 12 & 63, r[t++] = 128 | l >> 6 & 63, r[t++] = 128 | l & 63, s++;
                }
            }
            return r[t] = 0, t - i;
        }, hr = (e, r, t)=>{
            var n = ue(e) + 1, i = new Array(n), a = at(e, i, 0, i.length);
            return i.length = a, i;
        }, Rn = ()=>{
            if (!vr.length) {
                var e = null;
                if (T) {
                    var r = 256, t = Buffer.alloc(r), n = 0, i = process.stdin.fd;
                    try {
                        n = Ce.readSync(i, t, 0, r);
                    } catch (a) {
                        if (a.toString().includes("EOF")) n = 0;
                        else throw a;
                    }
                    n > 0 && (e = t.slice(0, n).toString("utf-8"));
                } else globalThis.window?.prompt && (e = window.prompt("Input: "), e !== null && (e += `
`));
                if (!e) return null;
                vr = hr(e);
            }
            return vr.shift();
        }, ae = {
            ttys: [],
            init () {},
            shutdown () {},
            register (e, r) {
                ae.ttys[e] = {
                    input: [],
                    output: [],
                    ops: r
                }, o.registerDevice(e, ae.stream_ops);
            },
            stream_ops: {
                open (e) {
                    var r = ae.ttys[e.node.rdev];
                    if (!r) throw new o.ErrnoError(43);
                    e.tty = r, e.seekable = !1;
                },
                close (e) {
                    e.tty.ops.fsync(e.tty);
                },
                fsync (e) {
                    e.tty.ops.fsync(e.tty);
                },
                read (e, r, t, n, i) {
                    if (!e.tty || !e.tty.ops.get_char) throw new o.ErrnoError(60);
                    for(var a = 0, s = 0; s < n; s++){
                        var l;
                        try {
                            l = e.tty.ops.get_char(e.tty);
                        } catch  {
                            throw new o.ErrnoError(29);
                        }
                        if (l === void 0 && a === 0) throw new o.ErrnoError(6);
                        if (l == null) break;
                        a++, r[t + s] = l;
                    }
                    return a && (e.node.atime = Date.now()), a;
                },
                write (e, r, t, n, i) {
                    if (!e.tty || !e.tty.ops.put_char) throw new o.ErrnoError(60);
                    try {
                        for(var a = 0; a < n; a++)e.tty.ops.put_char(e.tty, r[t + a]);
                    } catch  {
                        throw new o.ErrnoError(29);
                    }
                    return n && (e.node.mtime = e.node.ctime = Date.now()), a;
                }
            },
            default_tty_ops: {
                get_char (e) {
                    return Rn();
                },
                put_char (e, r) {
                    r === null || r === 10 ? (ne(ye(e.output)), e.output = []) : r != 0 && e.output.push(r);
                },
                fsync (e) {
                    e.output?.length > 0 && (ne(ye(e.output)), e.output = []);
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
                    r === null || r === 10 ? (b(ye(e.output)), e.output = []) : r != 0 && e.output.push(r);
                },
                fsync (e) {
                    e.output?.length > 0 && (b(ye(e.output)), e.output = []);
                }
            }
        }, Nn = (e, r)=>(f(), H).fill(0, e, e + r), st = (e, r)=>(u(r, "alignment argument is required"), Math.ceil(e / r) * r), lt = (e)=>{
            e = st(e, 65536);
            var r = Xt(65536, e);
            return r && Nn(r, e), r;
        }, E = {
            ops_table: null,
            mount (e) {
                return E.createNode(null, "/", 16895, 0);
            },
            createNode (e, r, t, n) {
                if (o.isBlkdev(t) || o.isFIFO(t)) throw new o.ErrnoError(63);
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
                        stream: o.chrdev_stream_ops
                    }
                };
                var i = o.createNode(e, r, t, n);
                return o.isDir(i.mode) ? (i.node_ops = E.ops_table.dir.node, i.stream_ops = E.ops_table.dir.stream, i.contents = {}) : o.isFile(i.mode) ? (i.node_ops = E.ops_table.file.node, i.stream_ops = E.ops_table.file.stream, i.usedBytes = 0, i.contents = null) : o.isLink(i.mode) ? (i.node_ops = E.ops_table.link.node, i.stream_ops = E.ops_table.link.stream) : o.isChrdev(i.mode) && (i.node_ops = E.ops_table.chrdev.node, i.stream_ops = E.ops_table.chrdev.stream), i.atime = i.mtime = i.ctime = Date.now(), e && (e.contents[r] = i, e.atime = e.mtime = e.ctime = i.atime), i;
            },
            getFileDataAsTypedArray (e) {
                return e.contents ? e.contents.subarray ? e.contents.subarray(0, e.usedBytes) : new Uint8Array(e.contents) : new Uint8Array(0);
            },
            expandFileStorage (e, r) {
                var t = e.contents ? e.contents.length : 0;
                if (!(t >= r)) {
                    var n = 1024 * 1024;
                    r = Math.max(r, t * (t < n ? 2 : 1.125) >>> 0), t != 0 && (r = Math.max(r, 256));
                    var i = e.contents;
                    e.contents = new Uint8Array(r), e.usedBytes > 0 && e.contents.set(i.subarray(0, e.usedBytes), 0);
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
                    return r.dev = o.isChrdev(e.mode) ? e.id : 1, r.ino = e.id, r.mode = e.mode, r.nlink = 1, r.uid = 0, r.gid = 0, r.rdev = e.rdev, o.isDir(e.mode) ? r.size = 4096 : o.isFile(e.mode) ? r.size = e.usedBytes : o.isLink(e.mode) ? r.size = e.link.length : r.size = 0, r.atime = new Date(e.atime), r.mtime = new Date(e.mtime), r.ctime = new Date(e.ctime), r.blksize = 4096, r.blocks = Math.ceil(r.size / r.blksize), r;
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
                    throw new o.ErrnoError(44);
                },
                mknod (e, r, t, n) {
                    return E.createNode(e, r, t, n);
                },
                rename (e, r, t) {
                    var n;
                    try {
                        n = o.lookupNode(r, t);
                    } catch  {}
                    if (n) {
                        if (o.isDir(e.mode)) for(var i in n.contents)throw new o.ErrnoError(55);
                        o.hashRemoveNode(n);
                    }
                    delete e.parent.contents[e.name], r.contents[t] = e, e.name = t, r.ctime = r.mtime = e.parent.ctime = e.parent.mtime = Date.now();
                },
                unlink (e, r) {
                    delete e.contents[r], e.ctime = e.mtime = Date.now();
                },
                rmdir (e, r) {
                    var t = o.lookupNode(e, r);
                    for(var n in t.contents)throw new o.ErrnoError(55);
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
                    if (!o.isLink(e.mode)) throw new o.ErrnoError(28);
                    return e.link;
                }
            },
            stream_ops: {
                read (e, r, t, n, i) {
                    var a = e.node.contents;
                    if (i >= e.node.usedBytes) return 0;
                    var s = Math.min(e.node.usedBytes - i, n);
                    if (u(s >= 0), s > 8 && a.subarray) r.set(a.subarray(i, i + s), t);
                    else for(var l = 0; l < s; l++)r[t + l] = a[i + l];
                    return s;
                },
                write (e, r, t, n, i, a) {
                    if (u(!(r instanceof ArrayBuffer)), r.buffer === (f(), U).buffer && (a = !1), !n) return 0;
                    var s = e.node;
                    if (s.mtime = s.ctime = Date.now(), r.subarray && (!s.contents || s.contents.subarray)) {
                        if (a) return u(i === 0, "canOwn must imply no weird position inside the file"), s.contents = r.subarray(t, t + n), s.usedBytes = n, n;
                        if (s.usedBytes === 0 && i === 0) return s.contents = r.slice(t, t + n), s.usedBytes = n, n;
                        if (i + n <= s.usedBytes) return s.contents.set(r.subarray(t, t + n), i), n;
                    }
                    if (E.expandFileStorage(s, i + n), s.contents.subarray && r.subarray) s.contents.set(r.subarray(t, t + n), i);
                    else for(var l = 0; l < n; l++)s.contents[i + l] = r[t + l];
                    return s.usedBytes = Math.max(s.usedBytes, i + n), n;
                },
                llseek (e, r, t) {
                    var n = r;
                    if (t === 1 ? n += e.position : t === 2 && o.isFile(e.node.mode) && (n += e.node.usedBytes), n < 0) throw new o.ErrnoError(28);
                    return n;
                },
                mmap (e, r, t, n, i) {
                    if (!o.isFile(e.node.mode)) throw new o.ErrnoError(43);
                    var a, s, l = e.node.contents;
                    if (!(i & 2) && l && l.buffer === (f(), U).buffer) s = !1, a = l.byteOffset;
                    else {
                        if (s = !0, a = lt(r), !a) throw new o.ErrnoError(48);
                        l && ((t > 0 || t + r < l.length) && (l.subarray ? l = l.subarray(t, t + r) : l = Array.prototype.slice.call(l, t, t + r)), (f(), U).set(l, a));
                    }
                    return {
                        ptr: a,
                        allocated: s
                    };
                },
                msync (e, r, t, n, i) {
                    return E.stream_ops.write(e, r, 0, n, t, !1), 0;
                }
            }
        }, On = (e)=>{
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
        }, pr = (e, r)=>{
            var t = 0;
            return e && (t |= 365), r && (t |= 146), t;
        }, Wn = (e)=>Z(Kt(e)), dt = {
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
        }, xn = async (e)=>{
            var r = await Ue(e);
            return u(r, `Loading data file "${e}" failed (no arrayBuffer).`), new Uint8Array(r);
        }, Ln = (...e)=>o.createDataFile(...e), Un = (e)=>{
            for(var r = e;;){
                if (!pe[e]) return e;
                e = r + Math.random();
            }
        }, ct = [], $n = async (e, r)=>{
            typeof Browser < "u" && Browser.init();
            for (var t of ct)if (t.canHandle(r)) return u(t.handle.constructor.name === "AsyncFunction", "Filesystem plugin handlers must be async functions (See #24914)"), t.handle(e, r);
            return e;
        }, ut = async (e, r, t, n, i, a, s, l)=>{
            var d = r ? we.resolve(F.join2(e, r)) : e, m = Un(`cp ${d}`);
            qr(m);
            try {
                var v = t;
                typeof t == "string" && (v = await xn(t)), v = await $n(v, d), l?.(), a || Ln(e, r, v, n, i, s);
            } finally{
                Xr(m);
            }
        }, Bn = (e, r, t, n, i, a, s, l, d, m)=>{
            ut(e, r, t, n, i, l, d, m).then(a).catch(s);
        }, o = {
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
                    super(he ? Wn(e) : ""), this.errno = e;
                    for(var r in dt)if (dt[r] === e) {
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
            },
            lookupPath (e, r = {}) {
                if (!e) throw new o.ErrnoError(44);
                r.follow_mount ??= !0, F.isAbs(e) || (e = o.cwd() + "/" + e);
                e: for(var t = 0; t < 40; t++){
                    for(var n = e.split("/").filter((m)=>!!m), i = o.root, a = "/", s = 0; s < n.length; s++){
                        var l = s === n.length - 1;
                        if (l && r.parent) break;
                        if (n[s] !== ".") {
                            if (n[s] === "..") {
                                if (a = F.dirname(a), o.isRoot(i)) {
                                    e = a + "/" + n.slice(s + 1).join("/"), t--;
                                    continue e;
                                } else i = i.parent;
                                continue;
                            }
                            a = F.join2(a, n[s]);
                            try {
                                i = o.lookupNode(i, n[s]);
                            } catch (m) {
                                if (m?.errno === 44 && l && r.noent_okay) return {
                                    path: a
                                };
                                throw m;
                            }
                            if (o.isMountpoint(i) && (!l || r.follow_mount) && (i = i.mounted.root), o.isLink(i.mode) && (!l || r.follow)) {
                                if (!i.node_ops.readlink) throw new o.ErrnoError(52);
                                var d = i.node_ops.readlink(i);
                                F.isAbs(d) || (d = F.dirname(a) + "/" + d), e = d + "/" + n.slice(s + 1).join("/");
                                continue e;
                            }
                        }
                    }
                    return {
                        path: a,
                        node: i
                    };
                }
                throw new o.ErrnoError(32);
            },
            getPath (e) {
                for(var r;;){
                    if (o.isRoot(e)) {
                        var t = e.mount.mountpoint;
                        return r ? t[t.length - 1] !== "/" ? `${t}/${r}` : t + r : t;
                    }
                    r = r ? `${e.name}/${r}` : e.name, e = e.parent;
                }
            },
            hashName (e, r) {
                for(var t = 0, n = 0; n < r.length; n++)t = (t << 5) - t + r.charCodeAt(n) | 0;
                return (e + t >>> 0) % o.nameTable.length;
            },
            hashAddNode (e) {
                var r = o.hashName(e.parent.id, e.name);
                e.name_next = o.nameTable[r], o.nameTable[r] = e;
            },
            hashRemoveNode (e) {
                var r = o.hashName(e.parent.id, e.name);
                if (o.nameTable[r] === e) o.nameTable[r] = e.name_next;
                else for(var t = o.nameTable[r]; t;){
                    if (t.name_next === e) {
                        t.name_next = e.name_next;
                        break;
                    }
                    t = t.name_next;
                }
            },
            lookupNode (e, r) {
                var t = o.mayLookup(e);
                if (t) throw new o.ErrnoError(t);
                for(var n = o.hashName(e.id, r), i = o.nameTable[n]; i; i = i.name_next){
                    var a = i.name;
                    if (i.parent.id === e.id && a === r) return i;
                }
                return o.lookup(e, r);
            },
            createNode (e, r, t, n) {
                u(typeof e == "object");
                var i = new o.FSNode(e, r, t, n);
                return o.hashAddNode(i), i;
            },
            destroyNode (e) {
                o.hashRemoveNode(e);
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
                return o.ignorePermissions ? 0 : r.includes("r") && !(e.mode & 292) || r.includes("w") && !(e.mode & 146) || r.includes("x") && !(e.mode & 73) ? 2 : 0;
            },
            mayLookup (e) {
                if (!o.isDir(e.mode)) return 54;
                var r = o.nodePermissions(e, "x");
                return r || (e.node_ops.lookup ? 0 : 2);
            },
            mayCreate (e, r) {
                if (!o.isDir(e.mode)) return 54;
                try {
                    var t = o.lookupNode(e, r);
                    return 20;
                } catch  {}
                return o.nodePermissions(e, "wx");
            },
            mayDelete (e, r, t) {
                var n;
                try {
                    n = o.lookupNode(e, r);
                } catch (a) {
                    return a.errno;
                }
                var i = o.nodePermissions(e, "wx");
                if (i) return i;
                if (t) {
                    if (!o.isDir(n.mode)) return 54;
                    if (o.isRoot(n) || o.getPath(n) === o.cwd()) return 10;
                } else if (o.isDir(n.mode)) return 31;
                return 0;
            },
            mayOpen (e, r) {
                return e ? o.isLink(e.mode) ? 32 : o.isDir(e.mode) && (o.flagsToPermissionString(r) !== "r" || r & 576) ? 31 : o.nodePermissions(e, o.flagsToPermissionString(r)) : 44;
            },
            checkOpExists (e, r) {
                if (!e) throw new o.ErrnoError(r);
                return e;
            },
            MAX_OPEN_FDS: 4096,
            nextfd () {
                for(var e = 0; e <= o.MAX_OPEN_FDS; e++)if (!o.streams[e]) return e;
                throw new o.ErrnoError(33);
            },
            getStreamChecked (e) {
                var r = o.getStream(e);
                if (!r) throw new o.ErrnoError(8);
                return r;
            },
            getStream: (e)=>o.streams[e],
            createStream (e, r = -1) {
                return u(r >= -1), e = Object.assign(new o.FSStream(), e), r == -1 && (r = o.nextfd()), e.fd = r, o.streams[r] = e, e;
            },
            closeStream (e) {
                o.streams[e] = null;
            },
            dupStream (e, r = -1) {
                var t = o.createStream(e, r);
                return t.stream_ops?.dup?.(t), t;
            },
            doSetAttr (e, r, t) {
                var n = e?.stream_ops.setattr, i = n ? e : r;
                n ??= r.node_ops.setattr, o.checkOpExists(n, 63), n(i, t);
            },
            chrdev_stream_ops: {
                open (e) {
                    var r = o.getDevice(e.node.rdev);
                    e.stream_ops = r.stream_ops, e.stream_ops.open?.(e);
                },
                llseek () {
                    throw new o.ErrnoError(70);
                }
            },
            major: (e)=>e >> 8,
            minor: (e)=>e & 255,
            makedev: (e, r)=>e << 8 | r,
            registerDevice (e, r) {
                o.devices[e] = {
                    stream_ops: r
                };
            },
            getDevice: (e)=>o.devices[e],
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
                typeof e == "function" && (r = e, e = !1), o.syncFSRequests++, o.syncFSRequests > 1 && b(`warning: ${o.syncFSRequests} FS.syncfs operations in flight at once, probably just doing extra work`);
                var t = o.getMounts(o.root.mount), n = 0;
                function i(l) {
                    return u(o.syncFSRequests > 0), o.syncFSRequests--, r(l);
                }
                function a(l) {
                    if (l) return a.errored ? void 0 : (a.errored = !0, i(l));
                    ++n >= t.length && i(null);
                }
                for (var s of t)s.type.syncfs ? s.type.syncfs(s, e, a) : a(null);
            },
            mount (e, r, t) {
                if (typeof e == "string") throw e;
                var n = t === "/", i = !t, a;
                if (n && o.root) throw new o.ErrnoError(10);
                if (!n && !i) {
                    var s = o.lookupPath(t, {
                        follow_mount: !1
                    });
                    if (t = s.path, a = s.node, o.isMountpoint(a)) throw new o.ErrnoError(10);
                    if (!o.isDir(a.mode)) throw new o.ErrnoError(54);
                }
                var l = {
                    type: e,
                    opts: r,
                    mountpoint: t,
                    mounts: []
                }, d = e.mount(l);
                return d.mount = l, l.root = d, n ? o.root = d : a && (a.mounted = l, a.mount && a.mount.mounts.push(l)), d;
            },
            unmount (e) {
                var r = o.lookupPath(e, {
                    follow_mount: !1
                });
                if (!o.isMountpoint(r.node)) throw new o.ErrnoError(28);
                var t = r.node, n = t.mounted, i = o.getMounts(n);
                for (var [a, s] of Object.entries(o.nameTable))for(; s;){
                    var l = s.name_next;
                    i.includes(s.mount) && o.destroyNode(s), s = l;
                }
                t.mounted = null;
                var d = t.mount.mounts.indexOf(n);
                u(d !== -1), t.mount.mounts.splice(d, 1);
            },
            lookup (e, r) {
                return e.node_ops.lookup(e, r);
            },
            mknod (e, r, t) {
                var n = o.lookupPath(e, {
                    parent: !0
                }), i = n.node, a = F.basename(e);
                if (!a) throw new o.ErrnoError(28);
                if (a === "." || a === "..") throw new o.ErrnoError(20);
                var s = o.mayCreate(i, a);
                if (s) throw new o.ErrnoError(s);
                if (!i.node_ops.mknod) throw new o.ErrnoError(63);
                return i.node_ops.mknod(i, a, r, t);
            },
            statfs (e) {
                return o.statfsNode(o.lookupPath(e, {
                    follow: !0
                }).node);
            },
            statfsStream (e) {
                return o.statfsNode(e.node);
            },
            statfsNode (e) {
                var r = {
                    bsize: 4096,
                    frsize: 4096,
                    blocks: 1e6,
                    bfree: 5e5,
                    bavail: 5e5,
                    files: o.nextInode,
                    ffree: o.nextInode - 1,
                    fsid: 42,
                    flags: 2,
                    namelen: 255
                };
                return e.node_ops.statfs && Object.assign(r, e.node_ops.statfs(e.mount.opts.root)), r;
            },
            create (e, r = 438) {
                return r &= 4095, r |= 32768, o.mknod(e, r, 0);
            },
            mkdir (e, r = 511) {
                return r &= 1023, r |= 16384, o.mknod(e, r, 0);
            },
            mkdirTree (e, r) {
                var t = e.split("/"), n = "";
                for (var i of t)if (i) {
                    (n || F.isAbs(e)) && (n += "/"), n += i;
                    try {
                        o.mkdir(n, r);
                    } catch (a) {
                        if (a.errno != 20) throw a;
                    }
                }
            },
            mkdev (e, r, t) {
                return typeof t > "u" && (t = r, r = 438), r |= 8192, o.mknod(e, r, t);
            },
            symlink (e, r) {
                if (!we.resolve(e)) throw new o.ErrnoError(44);
                var t = o.lookupPath(r, {
                    parent: !0
                }), n = t.node;
                if (!n) throw new o.ErrnoError(44);
                var i = F.basename(r), a = o.mayCreate(n, i);
                if (a) throw new o.ErrnoError(a);
                if (!n.node_ops.symlink) throw new o.ErrnoError(63);
                return n.node_ops.symlink(n, i, e);
            },
            rename (e, r) {
                var t = F.dirname(e), n = F.dirname(r), i = F.basename(e), a = F.basename(r), s, l, d;
                if (s = o.lookupPath(e, {
                    parent: !0
                }), l = s.node, s = o.lookupPath(r, {
                    parent: !0
                }), d = s.node, !l || !d) throw new o.ErrnoError(44);
                if (l.mount !== d.mount) throw new o.ErrnoError(75);
                var m = o.lookupNode(l, i), v = we.relative(e, n);
                if (v.charAt(0) !== ".") throw new o.ErrnoError(28);
                if (v = we.relative(r, t), v.charAt(0) !== ".") throw new o.ErrnoError(55);
                var g;
                try {
                    g = o.lookupNode(d, a);
                } catch  {}
                if (m !== g) {
                    var _ = o.isDir(m.mode), p = o.mayDelete(l, i, _);
                    if (p) throw new o.ErrnoError(p);
                    if (p = g ? o.mayDelete(d, a, _) : o.mayCreate(d, a), p) throw new o.ErrnoError(p);
                    if (!l.node_ops.rename) throw new o.ErrnoError(63);
                    if (o.isMountpoint(m) || g && o.isMountpoint(g)) throw new o.ErrnoError(10);
                    if (d !== l && (p = o.nodePermissions(l, "w"), p)) throw new o.ErrnoError(p);
                    o.hashRemoveNode(m);
                    try {
                        l.node_ops.rename(m, d, a), m.parent = d;
                    } catch (k) {
                        throw k;
                    } finally{
                        o.hashAddNode(m);
                    }
                }
            },
            rmdir (e) {
                var r = o.lookupPath(e, {
                    parent: !0
                }), t = r.node, n = F.basename(e), i = o.lookupNode(t, n), a = o.mayDelete(t, n, !0);
                if (a) throw new o.ErrnoError(a);
                if (!t.node_ops.rmdir) throw new o.ErrnoError(63);
                if (o.isMountpoint(i)) throw new o.ErrnoError(10);
                t.node_ops.rmdir(t, n), o.destroyNode(i);
            },
            readdir (e) {
                var r = o.lookupPath(e, {
                    follow: !0
                }), t = r.node, n = o.checkOpExists(t.node_ops.readdir, 54);
                return n(t);
            },
            unlink (e) {
                var r = o.lookupPath(e, {
                    parent: !0
                }), t = r.node;
                if (!t) throw new o.ErrnoError(44);
                var n = F.basename(e), i = o.lookupNode(t, n), a = o.mayDelete(t, n, !1);
                if (a) throw new o.ErrnoError(a);
                if (!t.node_ops.unlink) throw new o.ErrnoError(63);
                if (o.isMountpoint(i)) throw new o.ErrnoError(10);
                t.node_ops.unlink(t, n), o.destroyNode(i);
            },
            readlink (e) {
                var r = o.lookupPath(e), t = r.node;
                if (!t) throw new o.ErrnoError(44);
                if (!t.node_ops.readlink) throw new o.ErrnoError(28);
                return t.node_ops.readlink(t);
            },
            stat (e, r) {
                var t = o.lookupPath(e, {
                    follow: !r
                }), n = t.node, i = o.checkOpExists(n.node_ops.getattr, 63);
                return i(n);
            },
            fstat (e) {
                var r = o.getStreamChecked(e), t = r.node, n = r.stream_ops.getattr, i = n ? r : t;
                return n ??= t.node_ops.getattr, o.checkOpExists(n, 63), n(i);
            },
            lstat (e) {
                return o.stat(e, !0);
            },
            doChmod (e, r, t, n) {
                o.doSetAttr(e, r, {
                    mode: t & 4095 | r.mode & -4096,
                    ctime: Date.now(),
                    dontFollow: n
                });
            },
            chmod (e, r, t) {
                var n;
                if (typeof e == "string") {
                    var i = o.lookupPath(e, {
                        follow: !t
                    });
                    n = i.node;
                } else n = e;
                o.doChmod(null, n, r, t);
            },
            lchmod (e, r) {
                o.chmod(e, r, !0);
            },
            fchmod (e, r) {
                var t = o.getStreamChecked(e);
                o.doChmod(t, t.node, r, !1);
            },
            doChown (e, r, t) {
                o.doSetAttr(e, r, {
                    timestamp: Date.now(),
                    dontFollow: t
                });
            },
            chown (e, r, t, n) {
                var i;
                if (typeof e == "string") {
                    var a = o.lookupPath(e, {
                        follow: !n
                    });
                    i = a.node;
                } else i = e;
                o.doChown(null, i, n);
            },
            lchown (e, r, t) {
                o.chown(e, r, t, !0);
            },
            fchown (e, r, t) {
                var n = o.getStreamChecked(e);
                o.doChown(n, n.node, !1);
            },
            doTruncate (e, r, t) {
                if (o.isDir(r.mode)) throw new o.ErrnoError(31);
                if (!o.isFile(r.mode)) throw new o.ErrnoError(28);
                var n = o.nodePermissions(r, "w");
                if (n) throw new o.ErrnoError(n);
                o.doSetAttr(e, r, {
                    size: t,
                    timestamp: Date.now()
                });
            },
            truncate (e, r) {
                if (r < 0) throw new o.ErrnoError(28);
                var t;
                if (typeof e == "string") {
                    var n = o.lookupPath(e, {
                        follow: !0
                    });
                    t = n.node;
                } else t = e;
                o.doTruncate(null, t, r);
            },
            ftruncate (e, r) {
                var t = o.getStreamChecked(e);
                if (r < 0 || (t.flags & 2097155) === 0) throw new o.ErrnoError(28);
                o.doTruncate(t, t.node, r);
            },
            utime (e, r, t) {
                var n = o.lookupPath(e, {
                    follow: !0
                }), i = n.node, a = o.checkOpExists(i.node_ops.setattr, 63);
                a(i, {
                    atime: r,
                    mtime: t
                });
            },
            open (e, r, t = 438) {
                if (e === "") throw new o.ErrnoError(44);
                r = typeof r == "string" ? On(r) : r, r & 64 ? t = t & 4095 | 32768 : t = 0;
                var n, i;
                if (typeof e == "object") n = e;
                else {
                    i = e.endsWith("/");
                    var a = o.lookupPath(e, {
                        follow: !(r & 131072),
                        noent_okay: !0
                    });
                    n = a.node, e = a.path;
                }
                var s = !1;
                if (r & 64) if (n) {
                    if (r & 128) throw new o.ErrnoError(20);
                } else {
                    if (i) throw new o.ErrnoError(31);
                    n = o.mknod(e, t | 511, 0), s = !0;
                }
                if (!n) throw new o.ErrnoError(44);
                if (o.isChrdev(n.mode) && (r &= -513), r & 65536 && !o.isDir(n.mode)) throw new o.ErrnoError(54);
                if (!s) {
                    var l = o.mayOpen(n, r);
                    if (l) throw new o.ErrnoError(l);
                }
                r & 512 && !s && o.truncate(n, 0), r &= -131713;
                var d = o.createStream({
                    node: n,
                    path: o.getPath(n),
                    flags: r,
                    seekable: !0,
                    position: 0,
                    stream_ops: n.stream_ops,
                    ungotten: [],
                    error: !1
                });
                return d.stream_ops.open && d.stream_ops.open(d), s && o.chmod(n, t & 511), c.logReadFiles && !(r & 1) && (e in o.readFiles || (o.readFiles[e] = 1)), d;
            },
            close (e) {
                if (o.isClosed(e)) throw new o.ErrnoError(8);
                e.getdents && (e.getdents = null);
                try {
                    e.stream_ops.close && e.stream_ops.close(e);
                } catch (r) {
                    throw r;
                } finally{
                    o.closeStream(e.fd);
                }
                e.fd = null;
            },
            isClosed (e) {
                return e.fd === null;
            },
            llseek (e, r, t) {
                if (o.isClosed(e)) throw new o.ErrnoError(8);
                if (!e.seekable || !e.stream_ops.llseek) throw new o.ErrnoError(70);
                if (t != 0 && t != 1 && t != 2) throw new o.ErrnoError(28);
                return e.position = e.stream_ops.llseek(e, r, t), e.ungotten = [], e.position;
            },
            read (e, r, t, n, i) {
                if (u(t >= 0), n < 0 || i < 0) throw new o.ErrnoError(28);
                if (o.isClosed(e)) throw new o.ErrnoError(8);
                if ((e.flags & 2097155) === 1) throw new o.ErrnoError(8);
                if (o.isDir(e.node.mode)) throw new o.ErrnoError(31);
                if (!e.stream_ops.read) throw new o.ErrnoError(28);
                var a = typeof i < "u";
                if (!a) i = e.position;
                else if (!e.seekable) throw new o.ErrnoError(70);
                var s = e.stream_ops.read(e, r, t, n, i);
                return a || (e.position += s), s;
            },
            write (e, r, t, n, i, a) {
                if (u(t >= 0), n < 0 || i < 0) throw new o.ErrnoError(28);
                if (o.isClosed(e)) throw new o.ErrnoError(8);
                if ((e.flags & 2097155) === 0) throw new o.ErrnoError(8);
                if (o.isDir(e.node.mode)) throw new o.ErrnoError(31);
                if (!e.stream_ops.write) throw new o.ErrnoError(28);
                e.seekable && e.flags & 1024 && o.llseek(e, 0, 2);
                var s = typeof i < "u";
                if (!s) i = e.position;
                else if (!e.seekable) throw new o.ErrnoError(70);
                var l = e.stream_ops.write(e, r, t, n, i, a);
                return s || (e.position += l), l;
            },
            mmap (e, r, t, n, i) {
                if ((n & 2) !== 0 && (i & 2) === 0 && (e.flags & 2097155) !== 2) throw new o.ErrnoError(2);
                if ((e.flags & 2097155) === 1) throw new o.ErrnoError(2);
                if (!e.stream_ops.mmap) throw new o.ErrnoError(43);
                if (!r) throw new o.ErrnoError(28);
                return e.stream_ops.mmap(e, r, t, n, i);
            },
            msync (e, r, t, n, i) {
                return u(t >= 0), e.stream_ops.msync ? e.stream_ops.msync(e, r, t, n, i) : 0;
            },
            ioctl (e, r, t) {
                if (!e.stream_ops.ioctl) throw new o.ErrnoError(59);
                return e.stream_ops.ioctl(e, r, t);
            },
            readFile (e, r = {}) {
                r.flags = r.flags || 0, r.encoding = r.encoding || "binary", r.encoding !== "utf8" && r.encoding !== "binary" && C(`Invalid encoding type "${r.encoding}"`);
                var t = o.open(e, r.flags), n = o.stat(e), i = n.size, a = new Uint8Array(i);
                return o.read(t, a, 0, i, 0), r.encoding === "utf8" && (a = ye(a)), o.close(t), a;
            },
            writeFile (e, r, t = {}) {
                t.flags = t.flags || 577;
                var n = o.open(e, t.flags, t.mode);
                typeof r == "string" && (r = new Uint8Array(hr(r))), ArrayBuffer.isView(r) ? o.write(n, r, 0, r.byteLength, void 0, t.canOwn) : C("Unsupported data type"), o.close(n);
            },
            cwd: ()=>o.currentPath,
            chdir (e) {
                var r = o.lookupPath(e, {
                    follow: !0
                });
                if (r.node === null) throw new o.ErrnoError(44);
                if (!o.isDir(r.node.mode)) throw new o.ErrnoError(54);
                var t = o.nodePermissions(r.node, "x");
                if (t) throw new o.ErrnoError(t);
                o.currentPath = r.path;
            },
            createDefaultDirectories () {
                o.mkdir("/tmp"), o.mkdir("/home"), o.mkdir("/home/web_user");
            },
            createDefaultDevices () {
                o.mkdir("/dev"), o.registerDevice(o.makedev(1, 3), {
                    read: ()=>0,
                    write: (n, i, a, s, l)=>s,
                    llseek: ()=>0
                }), o.mkdev("/dev/null", o.makedev(1, 3)), ae.register(o.makedev(5, 0), ae.default_tty_ops), ae.register(o.makedev(6, 0), ae.default_tty1_ops), o.mkdev("/dev/tty", o.makedev(5, 0)), o.mkdev("/dev/tty1", o.makedev(6, 0));
                var e = new Uint8Array(1024), r = 0, t = ()=>(r === 0 && (_r(e), r = e.byteLength), e[--r]);
                o.createDevice("/dev", "random", t), o.createDevice("/dev", "urandom", t), o.mkdir("/dev/shm"), o.mkdir("/dev/shm/tmp");
            },
            createSpecialDirectories () {
                o.mkdir("/proc");
                var e = o.mkdir("/proc/self");
                o.mkdir("/proc/self/fd"), o.mount({
                    mount () {
                        var r = o.createNode(e, "fd", 16895, 73);
                        return r.stream_ops = {
                            llseek: E.stream_ops.llseek
                        }, r.node_ops = {
                            lookup (t, n) {
                                var i = +n, a = o.getStreamChecked(i), s = {
                                    parent: null,
                                    mount: {
                                        mountpoint: "fake"
                                    },
                                    node_ops: {
                                        readlink: ()=>a.path
                                    },
                                    id: i + 1
                                };
                                return s.parent = s, s;
                            },
                            readdir () {
                                return Array.from(o.streams.entries()).filter(([t, n])=>n).map(([t, n])=>t.toString());
                            }
                        }, r;
                    }
                }, {}, "/proc/self/fd");
            },
            createStandardStreams (e, r, t) {
                e ? o.createDevice("/dev", "stdin", e) : o.symlink("/dev/tty", "/dev/stdin"), r ? o.createDevice("/dev", "stdout", null, r) : o.symlink("/dev/tty", "/dev/stdout"), t ? o.createDevice("/dev", "stderr", null, t) : o.symlink("/dev/tty1", "/dev/stderr");
                var n = o.open("/dev/stdin", 0), i = o.open("/dev/stdout", 1), a = o.open("/dev/stderr", 1);
                u(n.fd === 0, `invalid handle for stdin (${n.fd})`), u(i.fd === 1, `invalid handle for stdout (${i.fd})`), u(a.fd === 2, `invalid handle for stderr (${a.fd})`);
            },
            staticInit () {
                o.nameTable = new Array(4096), o.mount(E, {}, "/"), o.createDefaultDirectories(), o.createDefaultDevices(), o.createSpecialDirectories(), o.filesystems = {
                    MEMFS: E
                };
            },
            init (e, r, t) {
                u(!o.initialized, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)"), o.initialized = !0, e ??= c.stdin, r ??= c.stdout, t ??= c.stderr, o.createStandardStreams(e, r, t);
            },
            quit () {
                o.initialized = !1, Sr(0);
                for (var e of o.streams)e && o.close(e);
            },
            findObject (e, r) {
                var t = o.analyzePath(e, r);
                return t.exists ? t.object : null;
            },
            analyzePath (e, r) {
                try {
                    var t = o.lookupPath(e, {
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
                    var t = o.lookupPath(e, {
                        parent: !0
                    });
                    n.parentExists = !0, n.parentPath = t.path, n.parentObject = t.node, n.name = F.basename(e), t = o.lookupPath(e, {
                        follow: !r
                    }), n.exists = !0, n.path = t.path, n.object = t.node, n.name = t.node.name, n.isRoot = t.path === "/";
                } catch (i) {
                    n.error = i.errno;
                }
                return n;
            },
            createPath (e, r, t, n) {
                e = typeof e == "string" ? e : o.getPath(e);
                for(var i = r.split("/").reverse(); i.length;){
                    var a = i.pop();
                    if (a) {
                        var s = F.join2(e, a);
                        try {
                            o.mkdir(s);
                        } catch (l) {
                            if (l.errno != 20) throw l;
                        }
                        e = s;
                    }
                }
                return s;
            },
            createFile (e, r, t, n, i) {
                var a = F.join2(typeof e == "string" ? e : o.getPath(e), r), s = pr(n, i);
                return o.create(a, s);
            },
            createDataFile (e, r, t, n, i, a) {
                var s = r;
                e && (e = typeof e == "string" ? e : o.getPath(e), s = r ? F.join2(e, r) : e);
                var l = pr(n, i), d = o.create(s, l);
                if (t) {
                    if (typeof t == "string") {
                        for(var m = new Array(t.length), v = 0, g = t.length; v < g; ++v)m[v] = t.charCodeAt(v);
                        t = m;
                    }
                    o.chmod(d, l | 146);
                    var _ = o.open(d, 577);
                    o.write(_, t, 0, t.length, 0, a), o.close(_), o.chmod(d, l);
                }
            },
            createDevice (e, r, t, n) {
                var i = F.join2(typeof e == "string" ? e : o.getPath(e), r), a = pr(!!t, !!n);
                o.createDevice.major ??= 64;
                var s = o.makedev(o.createDevice.major++, 0);
                return o.registerDevice(s, {
                    open (l) {
                        l.seekable = !1;
                    },
                    close (l) {
                        n?.buffer?.length && n(10);
                    },
                    read (l, d, m, v, g) {
                        for(var _ = 0, p = 0; p < v; p++){
                            var k;
                            try {
                                k = t();
                            } catch  {
                                throw new o.ErrnoError(29);
                            }
                            if (k === void 0 && _ === 0) throw new o.ErrnoError(6);
                            if (k == null) break;
                            _++, d[m + p] = k;
                        }
                        return _ && (l.node.atime = Date.now()), _;
                    },
                    write (l, d, m, v, g) {
                        for(var _ = 0; _ < v; _++)try {
                            n(d[m + _]);
                        } catch  {
                            throw new o.ErrnoError(29);
                        }
                        return v && (l.node.mtime = l.node.ctime = Date.now()), _;
                    }
                }), o.mkdev(i, a, s);
            },
            forceLoadFile (e) {
                if (e.isDevice || e.isFolder || e.link || e.contents) return !0;
                if (globalThis.XMLHttpRequest) C("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
                else try {
                    e.contents = Pe(e.url);
                } catch  {
                    throw new o.ErrnoError(29);
                }
            },
            createLazyFile (e, r, t, n, i) {
                class a {
                    lengthKnown = !1;
                    chunks = [];
                    get(_) {
                        if (!(_ > this.length - 1 || _ < 0)) {
                            var p = _ % this.chunkSize, k = _ / this.chunkSize | 0;
                            return this.getter(k)[p];
                        }
                    }
                    setDataGetter(_) {
                        this.getter = _;
                    }
                    cacheLength() {
                        var _ = new XMLHttpRequest();
                        _.open("HEAD", t, !1), _.send(null), _.status >= 200 && _.status < 300 || _.status === 304 || C("Couldn't load " + t + ". Status: " + _.status);
                        var p = Number(_.getResponseHeader("Content-length")), k, S = (k = _.getResponseHeader("Accept-Ranges")) && k === "bytes", N = (k = _.getResponseHeader("Content-Encoding")) && k === "gzip", j = 1024 * 1024;
                        S || (j = p);
                        var z = (Y, Fe)=>{
                            Y > Fe && C("invalid range (" + Y + ", " + Fe + ") or no bytes requested!"), Fe > p - 1 && C("only " + p + " bytes available! programmer error!");
                            var $ = new XMLHttpRequest();
                            return $.open("GET", t, !1), p !== j && $.setRequestHeader("Range", "bytes=" + Y + "-" + Fe), $.responseType = "arraybuffer", $.overrideMimeType && $.overrideMimeType("text/plain; charset=x-user-defined"), $.send(null), $.status >= 200 && $.status < 300 || $.status === 304 || C("Couldn't load " + t + ". Status: " + $.status), $.response !== void 0 ? new Uint8Array($.response || []) : hr($.responseText || "");
                        }, We = this;
                        We.setDataGetter((Y)=>{
                            var Fe = Y * j, $ = (Y + 1) * j - 1;
                            return $ = Math.min($, p - 1), typeof We.chunks[Y] > "u" && (We.chunks[Y] = z(Fe, $)), typeof We.chunks[Y] > "u" && C("doXHR failed!"), We.chunks[Y];
                        }), (N || !p) && (j = p = 1, p = this.getter(0).length, j = p, ne("LazyFiles on gzip forces download of the whole file when length is accessed")), this._length = p, this._chunkSize = j, this.lengthKnown = !0;
                    }
                    get length() {
                        return this.lengthKnown || this.cacheLength(), this._length;
                    }
                    get chunkSize() {
                        return this.lengthKnown || this.cacheLength(), this._chunkSize;
                    }
                }
                if (globalThis.XMLHttpRequest) {
                    L || C("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
                    var s = new a(), l = {
                        isDevice: !1,
                        contents: s
                    };
                } else var l = {
                    isDevice: !1,
                    url: t
                };
                var d = o.createFile(e, r, l, n, i);
                l.contents ? d.contents = l.contents : l.url && (d.contents = null, d.url = l.url), Object.defineProperties(d, {
                    usedBytes: {
                        get: function() {
                            return this.contents.length;
                        }
                    }
                });
                var m = {};
                for (const [g, _] of Object.entries(d.stream_ops))m[g] = (...p)=>(o.forceLoadFile(d), _(...p));
                function v(g, _, p, k, S) {
                    var N = g.node.contents;
                    if (S >= N.length) return 0;
                    var j = Math.min(N.length - S, k);
                    if (u(j >= 0), N.slice) for(var z = 0; z < j; z++)_[p + z] = N[S + z];
                    else for(var z = 0; z < j; z++)_[p + z] = N.get(S + z);
                    return j;
                }
                return m.read = (g, _, p, k, S)=>(o.forceLoadFile(d), v(g, _, p, k, S)), m.mmap = (g, _, p, k, S)=>{
                    o.forceLoadFile(d);
                    var N = lt(_);
                    if (!N) throw new o.ErrnoError(48);
                    return v(g, (f(), U), N, _, p), {
                        ptr: N,
                        allocated: !0
                    };
                }, d.stream_ops = m, d;
            },
            absolutePath () {
                C("FS.absolutePath has been removed; use PATH_FS.resolve instead");
            },
            createFolder () {
                C("FS.createFolder has been removed; use FS.mkdir instead");
            },
            createLink () {
                C("FS.createLink has been removed; use FS.symlink instead");
            },
            joinPath () {
                C("FS.joinPath has been removed; use PATH.join instead");
            },
            mmapAlloc () {
                C("FS.mmapAlloc has been replaced by the top level function mmapAlloc");
            },
            standardizePath () {
                C("FS.standardizePath has been removed; use PATH.normalize instead");
            }
        }, I = {
            DEFAULT_POLLMASK: 5,
            calculateAt (e, r, t) {
                if (F.isAbs(r)) return r;
                var n;
                if (e === -100) n = o.cwd();
                else {
                    var i = I.getStreamFromFD(e);
                    n = i.path;
                }
                if (r.length == 0) {
                    if (!t) throw new o.ErrnoError(44);
                    return n;
                }
                return n + "/" + r;
            },
            writeStat (e, r) {
                (f(), h)[e >> 2] = r.dev, (f(), h)[e + 4 >> 2] = r.mode, (f(), h)[e + 8 >> 2] = r.nlink, (f(), h)[e + 12 >> 2] = r.uid, (f(), h)[e + 16 >> 2] = r.gid, (f(), h)[e + 20 >> 2] = r.rdev, (f(), O)[e + 24 >> 3] = BigInt(r.size), (f(), R)[e + 32 >> 2] = 4096, (f(), R)[e + 36 >> 2] = r.blocks;
                var t = r.atime.getTime(), n = r.mtime.getTime(), i = r.ctime.getTime();
                return (f(), O)[e + 40 >> 3] = BigInt(Math.floor(t / 1e3)), (f(), h)[e + 48 >> 2] = t % 1e3 * 1e3 * 1e3, (f(), O)[e + 56 >> 3] = BigInt(Math.floor(n / 1e3)), (f(), h)[e + 64 >> 2] = n % 1e3 * 1e3 * 1e3, (f(), O)[e + 72 >> 3] = BigInt(Math.floor(i / 1e3)), (f(), h)[e + 80 >> 2] = i % 1e3 * 1e3 * 1e3, (f(), O)[e + 88 >> 3] = BigInt(r.ino), 0;
            },
            writeStatFs (e, r) {
                (f(), h)[e + 4 >> 2] = r.bsize, (f(), h)[e + 60 >> 2] = r.bsize, (f(), O)[e + 8 >> 3] = BigInt(r.blocks), (f(), O)[e + 16 >> 3] = BigInt(r.bfree), (f(), O)[e + 24 >> 3] = BigInt(r.bavail), (f(), O)[e + 32 >> 3] = BigInt(r.files), (f(), O)[e + 40 >> 3] = BigInt(r.ffree), (f(), h)[e + 48 >> 2] = r.fsid, (f(), h)[e + 64 >> 2] = r.flags, (f(), h)[e + 56 >> 2] = r.namelen;
            },
            doMsync (e, r, t, n, i) {
                if (!o.isFile(r.node.mode)) throw new o.ErrnoError(43);
                if (n & 2) return 0;
                var a = (f(), H).slice(e, e + t);
                o.msync(r, a, i, t, n);
            },
            getStreamFromFD (e) {
                var r = o.getStreamChecked(e);
                return r;
            },
            varargs: void 0,
            getStr (e) {
                var r = Z(e);
                return r;
            }
        };
        function ft(e, r, t) {
            if (y) return x(3, 0, 1, e, r, t);
            I.varargs = t;
            try {
                var n = I.getStreamFromFD(e);
                switch(r){
                    case 0:
                        {
                            var i = He();
                            if (i < 0) return -28;
                            for(; o.streams[i];)i++;
                            var a;
                            return a = o.dupStream(n, i), a.fd;
                        }
                    case 1:
                    case 2:
                        return 0;
                    case 3:
                        return n.flags;
                    case 4:
                        {
                            var i = He();
                            return n.flags |= i, 0;
                        }
                    case 12:
                        {
                            var i = Ee(), s = 0;
                            return (f(), le)[i + s >> 1] = 2, 0;
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
        function mt(e, r) {
            if (y) return x(4, 0, 1, e, r);
            try {
                return I.writeStat(r, o.fstat(e));
            } catch (t) {
                if (typeof o > "u" || t.name !== "ErrnoError") throw t;
                return -t.errno;
            }
        }
        function _t(e, r, t) {
            if (y) return x(5, 0, 1, e, r, t);
            I.varargs = t;
            try {
                var n = I.getStreamFromFD(e);
                switch(r){
                    case 21509:
                        return n.tty ? 0 : -59;
                    case 21505:
                        {
                            if (!n.tty) return -59;
                            if (n.tty.ops.ioctl_tcgets) {
                                var i = n.tty.ops.ioctl_tcgets(n), a = Ee();
                                (f(), R)[a >> 2] = i.c_iflag || 0, (f(), R)[a + 4 >> 2] = i.c_oflag || 0, (f(), R)[a + 8 >> 2] = i.c_cflag || 0, (f(), R)[a + 12 >> 2] = i.c_lflag || 0;
                                for(var s = 0; s < 32; s++)(f(), U)[a + s + 17] = i.c_cc[s] || 0;
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
                                for(var a = Ee(), l = (f(), R)[a >> 2], d = (f(), R)[a + 4 >> 2], m = (f(), R)[a + 8 >> 2], v = (f(), R)[a + 12 >> 2], g = [], s = 0; s < 32; s++)g.push((f(), U)[a + s + 17]);
                                return n.tty.ops.ioctl_tcsets(n.tty, r, {
                                    c_iflag: l,
                                    c_oflag: d,
                                    c_cflag: m,
                                    c_lflag: v,
                                    c_cc: g
                                });
                            }
                            return 0;
                        }
                    case 21519:
                        {
                            if (!n.tty) return -59;
                            var a = Ee();
                            return (f(), R)[a >> 2] = 0, 0;
                        }
                    case 21520:
                        return n.tty ? -28 : -59;
                    case 21537:
                    case 21531:
                        {
                            var a = Ee();
                            return o.ioctl(n, r, a);
                        }
                    case 21523:
                        {
                            if (!n.tty) return -59;
                            if (n.tty.ops.ioctl_tiocgwinsz) {
                                var _ = n.tty.ops.ioctl_tiocgwinsz(n.tty), a = Ee();
                                (f(), le)[a >> 1] = _[0], (f(), le)[a + 2 >> 1] = _[1];
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
        function vt(e, r) {
            if (y) return x(6, 0, 1, e, r);
            try {
                return e = I.getStr(e), I.writeStat(r, o.lstat(e));
            } catch (t) {
                if (typeof o > "u" || t.name !== "ErrnoError") throw t;
                return -t.errno;
            }
        }
        function ht(e, r, t, n) {
            if (y) return x(7, 0, 1, e, r, t, n);
            try {
                r = I.getStr(r);
                var i = n & 256, a = n & 4096;
                return n = n & -6401, u(!n, `unknown flags in __syscall_newfstatat: ${n}`), r = I.calculateAt(e, r, a), I.writeStat(t, i ? o.lstat(r) : o.stat(r));
            } catch (s) {
                if (typeof o > "u" || s.name !== "ErrnoError") throw s;
                return -s.errno;
            }
        }
        function pt(e, r, t, n) {
            if (y) return x(8, 0, 1, e, r, t, n);
            I.varargs = n;
            try {
                r = I.getStr(r), r = I.calculateAt(e, r);
                var i = n ? He() : 0;
                return o.open(r, t, i).fd;
            } catch (a) {
                if (typeof o > "u" || a.name !== "ErrnoError") throw a;
                return -a.errno;
            }
        }
        function gt(e, r) {
            if (y) return x(9, 0, 1, e, r);
            try {
                return e = I.getStr(e), I.writeStat(r, o.stat(e));
            } catch (t) {
                if (typeof o > "u" || t.name !== "ErrnoError") throw t;
                return -t.errno;
            }
        }
        var jn = ()=>C("native code called abort()"), V = (e)=>{
            for(var r = "";;){
                var t = (f(), H)[e++];
                if (!t) return r;
                r += String.fromCharCode(t);
            }
        }, be = {}, Se = {}, Ve = {}, zn = class extends Error {
            constructor(r){
                super(r), this.name = "BindingError";
            }
        }, G = (e)=>{
            throw new zn(e);
        };
        function Hn(e, r, t = {}) {
            var n = r.name;
            if (e || G(`type "${n}" must have a positive integer typeid pointer`), Se.hasOwnProperty(e)) {
                if (t.ignoreDuplicateRegistrations) return;
                G(`Cannot register type '${n}' twice`);
            }
            if (Se[e] = r, delete Ve[e], be.hasOwnProperty(e)) {
                var i = be[e];
                delete be[e], i.forEach((a)=>a());
            }
        }
        function X(e, r, t = {}) {
            return Hn(e, r, t);
        }
        var yt = (e, r, t)=>{
            switch(r){
                case 1:
                    return t ? (n)=>(f(), U)[n] : (n)=>(f(), H)[n];
                case 2:
                    return t ? (n)=>(f(), le)[n >> 1] : (n)=>(f(), De)[n >> 1];
                case 4:
                    return t ? (n)=>(f(), R)[n >> 2] : (n)=>(f(), h)[n >> 2];
                case 8:
                    return t ? (n)=>(f(), O)[n >> 3] : (n)=>(f(), Lr)[n >> 3];
                default:
                    throw new TypeError(`invalid integer width (${r}): ${e}`);
            }
        }, Ge = (e)=>{
            if (e === null) return "null";
            var r = typeof e;
            return r === "object" || r === "array" || r === "function" ? e.toString() : "" + e;
        }, Et = (e, r, t, n)=>{
            if (r < t || r > n) throw new TypeError(`Passing a number "${Ge(r)}" from JS side to C/C++ side to an argument of type "${e}", which is outside the valid range [${t}, ${n}]!`);
        }, Vn = (e, r, t, n, i)=>{
            r = V(r);
            const a = n === 0n;
            let s = (l)=>l;
            if (a) {
                const l = t * 8;
                s = (d)=>BigInt.asUintN(l, d), i = s(i);
            }
            X(e, {
                name: r,
                fromWireType: s,
                toWireType: (l, d)=>{
                    if (typeof d == "number") d = BigInt(d);
                    else if (typeof d != "bigint") throw new TypeError(`Cannot convert "${Ge(d)}" to ${this.name}`);
                    return Et(r, d, n, i), d;
                },
                readValueFromPointer: yt(r, t, !a),
                destructorFunction: null
            });
        }, Gn = (e, r, t, n)=>{
            r = V(r), X(e, {
                name: r,
                fromWireType: function(i) {
                    return !!i;
                },
                toWireType: function(i, a) {
                    return a ? t : n;
                },
                readValueFromPointer: function(i) {
                    return this.fromWireType((f(), H)[i]);
                },
                destructorFunction: null
            });
        }, wt = [], Q = [
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
        ], Kn = (e)=>{
            e > 9 && --Q[e + 1] === 0 && (u(Q[e] !== void 0, "Decref for unallocated handle."), Q[e] = void 0, wt.push(e));
        }, bt = {
            toValue: (e)=>(e || G(`Cannot use deleted val. handle = ${e}`), u(e === 2 || Q[e] !== void 0 && e % 2 === 0, `invalid handle: ${e}`), Q[e]),
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
                            const r = wt.pop() || Q.length;
                            return Q[r] = e, Q[r + 1] = 1, r;
                        }
                }
            }
        };
        function gr(e) {
            return this.fromWireType((f(), h)[e >> 2]);
        }
        var Xn = {
            name: "emscripten::val",
            fromWireType: (e)=>{
                var r = bt.toValue(e);
                return Kn(e), r;
            },
            toWireType: (e, r)=>bt.toHandle(r),
            readValueFromPointer: gr,
            destructorFunction: null
        }, qn = (e)=>X(e, Xn), Yn = (e, r)=>{
            switch(r){
                case 4:
                    return function(t) {
                        return this.fromWireType((f(), xr)[t >> 2]);
                    };
                case 8:
                    return function(t) {
                        return this.fromWireType((f(), Re)[t >> 3]);
                    };
                default:
                    throw new TypeError(`invalid float width (${r}): ${e}`);
            }
        }, Jn = (e, r, t)=>{
            r = V(r), X(e, {
                name: r,
                fromWireType: (n)=>n,
                toWireType: (n, i)=>{
                    if (typeof i != "number" && typeof i != "boolean") throw new TypeError(`Cannot convert ${Ge(i)} to ${this.name}`);
                    return i;
                },
                readValueFromPointer: Yn(r, t),
                destructorFunction: null
            });
        }, St = (e, r)=>Object.defineProperty(r, "name", {
                value: e
            }), Zn = (e)=>{
            for(; e.length;){
                var r = e.pop(), t = e.pop();
                t(r);
            }
        };
        function kt(e) {
            for(var r = 1; r < e.length; ++r)if (e[r] !== null && e[r].destructorFunction === void 0) return !0;
            return !1;
        }
        function Qn(e, r, t, n, i) {
            if (e < r || e > t) {
                var a = r == t ? r : `${r} to ${t}`;
                i(`function ${n} called with ${e} arguments, expected ${a}`);
            }
        }
        function eo(e, r, t, n) {
            for(var i = kt(e), a = e.length - 2, s = [], l = [
                "fn"
            ], d = 0; d < a; ++d)s.push(`arg${d}`), l.push(`arg${d}Wired`);
            s = s.join(","), l = l.join(",");
            var m = `return function (${s}) {
`;
            m += `checkArgCount(arguments.length, minArgs, maxArgs, humanName, throwBindingError);
`, i && (m += `var destructors = [];
`);
            for(var v = i ? "destructors" : "null", g = [
                "humanName",
                "throwBindingError",
                "invoker",
                "fn",
                "runDestructors",
                "fromRetWire",
                "toClassParamWire"
            ], d = 0; d < a; ++d){
                var _ = `toArg${d}Wire`;
                m += `var arg${d}Wired = ${_}(${v}, arg${d});
`, g.push(_);
            }
            m += (t || n ? "var rv = " : "") + `invoker(${l});
`;
            var p = t ? "rv" : "";
            if (m += `function onDone(${p}) {
`, i) m += `runDestructors(destructors);
`;
            else for(var d = 2; d < e.length; ++d){
                var k = d === 1 ? "thisWired" : "arg" + (d - 2) + "Wired";
                e[d].destructorFunction !== null && (m += `${k}_dtor(${k});
`, g.push(`${k}_dtor`));
            }
            return t && (m += `var ret = fromRetWire(rv);
return ret;
`), m += `}
`, m += "return " + (n ? "rv.then(onDone)" : `onDone(${p})`) + ";", m += `}
`, g.push("checkArgCount", "minArgs", "maxArgs"), m = `if (arguments.length !== ${g.length}){ throw new Error(humanName + "Expected ${g.length} closure arguments " + arguments.length + " given."); }
${m}`, new Function(g, m);
        }
        function ro(e) {
            for(var r = e.length - 2, t = e.length - 1; t >= 2 && e[t].optional; --t)r--;
            return r;
        }
        function to(e, r, t, n, i, a) {
            var s = r.length;
            s < 2 && G("argTypes array size mismatch! Must at least get return value and 'this' types!");
            for(var l = r[1] !== null && t !== null, d = kt(r), m = !r[0].isVoid, v = s - 2, g = ro(r), _ = r[0], p = r[1], k = [
                e,
                G,
                n,
                i,
                Zn,
                _.fromWireType.bind(_),
                p?.toWireType.bind(p)
            ], S = 2; S < s; ++S){
                var N = r[S];
                k.push(N.toWireType.bind(N));
            }
            if (!d) for(var S = 2; S < r.length; ++S)r[S].destructorFunction !== null && k.push(r[S].destructorFunction);
            k.push(Qn, g, v);
            var z = eo(r, l, m, a)(...k);
            return St(e, z);
        }
        var no = (e, r, t)=>{
            if (e[r].overloadTable === void 0) {
                var n = e[r];
                e[r] = function(...i) {
                    return e[r].overloadTable.hasOwnProperty(i.length) || G(`Function '${t}' called with an invalid number of arguments (${i.length}) - expects one of (${e[r].overloadTable})!`), e[r].overloadTable[i.length].apply(this, i);
                }, e[r].overloadTable = [], e[r].overloadTable[n.argCount] = n;
            }
        }, oo = (e, r, t)=>{
            c.hasOwnProperty(e) ? ((t === void 0 || c[e].overloadTable !== void 0 && c[e].overloadTable[t] !== void 0) && G(`Cannot register public name '${e}' twice`), no(c, e, e), c[e].overloadTable.hasOwnProperty(t) && G(`Cannot register multiple overloads of a function with the same number of arguments (${t})!`), c[e].overloadTable[t] = r) : (c[e] = r, c[e].argCount = t);
        }, io = (e, r)=>{
            for(var t = [], n = 0; n < e; n++)t.push((f(), h)[r + n * 4 >> 2]);
            return t;
        }, ao = class extends Error {
            constructor(r){
                super(r), this.name = "InternalError";
            }
        }, Tt = (e)=>{
            throw new ao(e);
        }, so = (e, r, t)=>{
            c.hasOwnProperty(e) || Tt("Replacing nonexistent public symbol"), c[e].overloadTable !== void 0 && t !== void 0 ? c[e].overloadTable[t] = r : (c[e] = r, c[e].argCount = t);
        }, lo = (e, r, t = !1)=>{
            e = V(e);
            function n() {
                var a = et(r);
                return t && (a = WebAssembly.promising(a)), a;
            }
            var i = n();
            return typeof i != "function" && G(`unknown function pointer with signature ${e}: ${r}`), i;
        };
        class co extends Error {
        }
        var uo = (e)=>{
            var r = Vt(e), t = V(r);
            return re(r), t;
        }, fo = (e, r)=>{
            var t = [], n = {};
            function i(a) {
                if (!n[a] && !Se[a]) {
                    if (Ve[a]) {
                        Ve[a].forEach(i);
                        return;
                    }
                    t.push(a), n[a] = !0;
                }
            }
            throw r.forEach(i), new co(`${e}: ` + t.map(uo).join([
                ", "
            ]));
        }, mo = (e, r, t)=>{
            e.forEach((l)=>Ve[l] = r);
            function n(l) {
                var d = t(l);
                d.length !== e.length && Tt("Mismatched type converter count");
                for(var m = 0; m < e.length; ++m)X(e[m], d[m]);
            }
            var i = new Array(r.length), a = [], s = 0;
            for (let [l, d] of r.entries())Se.hasOwnProperty(d) ? i[l] = Se[d] : (a.push(d), be.hasOwnProperty(d) || (be[d] = []), be[d].push(()=>{
                i[l] = Se[d], ++s, s === a.length && n(i);
            }));
            a.length === 0 && n(i);
        }, _o = (e)=>{
            e = e.trim();
            const r = e.indexOf("(");
            return r === -1 ? e : (u(e.endsWith(")"), "Parentheses for argument names should match."), e.slice(0, r));
        }, vo = (e, r, t, n, i, a, s, l)=>{
            var d = io(r, t);
            e = V(e), e = _o(e), i = lo(n, i, s), oo(e, function() {
                fo(`Cannot call ${e} due to unbound types`, d);
            }, r - 1), mo([], d, (m)=>{
                var v = [
                    m[0],
                    null
                ].concat(m.slice(1));
                return so(e, to(e, v, null, i, a, s), r - 1), [];
            });
        }, ho = (e, r, t, n, i)=>{
            r = V(r);
            const a = n === 0;
            let s = (d)=>d;
            if (a) {
                var l = 32 - 8 * t;
                s = (d)=>d << l >>> l, i = s(i);
            }
            X(e, {
                name: r,
                fromWireType: s,
                toWireType: (d, m)=>{
                    if (typeof m != "number" && typeof m != "boolean") throw new TypeError(`Cannot convert "${Ge(m)}" to ${r}`);
                    return Et(r, m, n, i), m;
                },
                readValueFromPointer: yt(r, t, n !== 0),
                destructorFunction: null
            });
        }, po = (e, r, t)=>{
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
            ], i = n[r];
            function a(s) {
                var l = (f(), h)[s >> 2], d = (f(), h)[s + 4 >> 2];
                return new i((f(), U).buffer, d, l);
            }
            t = V(t), X(e, {
                name: t,
                fromWireType: a,
                readValueFromPointer: a
            }, {
                ignoreDuplicateRegistrations: !0
            });
        }, ee = (e, r, t)=>(u(typeof t == "number", "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), at(e, (f(), H), r, t)), go = (e, r)=>{
            r = V(r), X(e, {
                name: r,
                fromWireType (t) {
                    var n = (f(), h)[t >> 2], i = t + 4, a;
                    return a = Z(i, n, !0), re(t), a;
                },
                toWireType (t, n) {
                    n instanceof ArrayBuffer && (n = new Uint8Array(n));
                    var i, a = typeof n == "string";
                    a || ArrayBuffer.isView(n) && n.BYTES_PER_ELEMENT == 1 || G("Cannot pass non-string to std::string"), a ? i = ue(n) : i = n.length;
                    var s = Ze(4 + i + 1), l = s + 4;
                    return (f(), h)[s >> 2] = i, a ? ee(n, l, i + 1) : (f(), H).set(n, l), t !== null && t.push(re, s), s;
                },
                readValueFromPointer: gr,
                destructorFunction (t) {
                    re(t);
                }
            });
        }, Ft = globalThis.TextDecoder ? new TextDecoder("utf-16le") : void 0, yo = (e, r, t)=>{
            u(e % 2 == 0, "Pointer passed to UTF16ToString must be aligned to two bytes!");
            var n = e >> 1, i = nt((f(), De), n, r / 2, t);
            if (i - n > 16 && Ft) return Ft.decode((f(), De).slice(n, i));
            for(var a = "", s = n; s < i; ++s){
                var l = (f(), De)[s];
                a += String.fromCharCode(l);
            }
            return a;
        }, Eo = (e, r, t)=>{
            if (u(r % 2 == 0, "Pointer passed to stringToUTF16 must be aligned to two bytes!"), u(typeof t == "number", "stringToUTF16(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 2) return 0;
            t -= 2;
            for(var n = r, i = t < e.length * 2 ? t / 2 : e.length, a = 0; a < i; ++a){
                var s = e.charCodeAt(a);
                (f(), le)[r >> 1] = s, r += 2;
            }
            return (f(), le)[r >> 1] = 0, r - n;
        }, wo = (e)=>e.length * 2, bo = (e, r, t)=>{
            u(e % 4 == 0, "Pointer passed to UTF32ToString must be aligned to four bytes!");
            for(var n = "", i = e >> 2, a = 0; !(a >= r / 4); a++){
                var s = (f(), h)[i + a];
                if (!s && !t) break;
                n += String.fromCodePoint(s);
            }
            return n;
        }, So = (e, r, t)=>{
            if (u(r % 4 == 0, "Pointer passed to stringToUTF32 must be aligned to four bytes!"), u(typeof t == "number", "stringToUTF32(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!"), t ??= 2147483647, t < 4) return 0;
            for(var n = r, i = n + t - 4, a = 0; a < e.length; ++a){
                var s = e.codePointAt(a);
                if (s > 65535 && a++, (f(), R)[r >> 2] = s, r += 4, r + 4 > i) break;
            }
            return (f(), R)[r >> 2] = 0, r - n;
        }, ko = (e)=>{
            for(var r = 0, t = 0; t < e.length; ++t){
                var n = e.codePointAt(t);
                n > 65535 && t++, r += 4;
            }
            return r;
        }, To = (e, r, t)=>{
            t = V(t);
            var n, i, a;
            r === 2 ? (n = yo, i = Eo, a = wo) : (u(r === 4, "only 2-byte and 4-byte strings are currently supported"), n = bo, i = So, a = ko), X(e, {
                name: t,
                fromWireType: (s)=>{
                    var l = (f(), h)[s >> 2], d = n(s + 4, l * r, !0);
                    return re(s), d;
                },
                toWireType: (s, l)=>{
                    typeof l != "string" && G(`Cannot pass non-string to C++ string type ${t}`);
                    var d = a(l), m = Ze(4 + d + r);
                    return (f(), h)[m >> 2] = d / r, i(l, m + 4, d + r), s !== null && s.push(re, m), m;
                },
                readValueFromPointer: gr,
                destructorFunction (s) {
                    re(s);
                }
            });
        }, Fo = (e, r)=>{
            r = V(r), X(e, {
                isVoid: !0,
                name: r,
                fromWireType: ()=>{},
                toWireType: (t, n)=>{}
            });
        }, Ao = (e)=>{
            kr(e, !L, 1, !B, 65536, !1), w.threadInitTLS();
        }, At = (e)=>{
            if (e instanceof jr || e == "unwind") return ve;
            Me(), e instanceof WebAssembly.RuntimeError && Ar() <= 0 && b("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), rr(1, e);
        }, Po = ()=>{
            if (!ze()) try {
                if (y) {
                    ke() && Fr(ve);
                    return;
                }
                ur(ve);
            } catch (e) {
                At(e);
            }
        }, Pt = (e)=>{
            if (_e) {
                b("user callback triggered after runtime exited or application aborted.  Ignoring.");
                return;
            }
            try {
                e(), Po();
            } catch (r) {
                At(r);
            }
        }, yr = (e)=>{
            if (Atomics.waitAsync) {
                var r = Atomics.waitAsync((f(), R), e >> 2, e);
                u(r.async), r.value.then(Ke);
                var t = e + 128;
                Atomics.store((f(), R), t >> 2, 1);
            }
        }, Ke = ()=>Pt(()=>{
                var e = ke();
                e && (yr(e), Zt());
            }), Co = (e, r)=>{
            if (e == r) setTimeout(Ke);
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
        }, Xe = [], Io = (e, r, t, n, i)=>{
            n /= 2, Xe.length = n;
            for(var a = i >> 3, s = 0; s < n; s++)(f(), O)[a + 2 * s] ? Xe[s] = (f(), O)[a + 2 * s + 1] : Xe[s] = (f(), Re)[a + 2 * s + 1];
            var l = r ? br[r] : di[e];
            u(!(e && r)), u(l.length == n, "Call args mismatch in _emscripten_receive_on_main_thread_js"), w.currentProxiedOperationCallerThread = t;
            var d = l(...Xe);
            return w.currentProxiedOperationCallerThread = 0, u(typeof d != "bigint"), d;
        }, Mo = (e)=>{
            y ? postMessage({
                cmd: "cleanupThread",
                thread: e
            }) : Hr(e);
        }, Do = (e)=>{
            T && w.pthreads[e].ref();
        }, Ro = 9007199254740992, No = -9007199254740992, Er = (e)=>e < No || e > Ro ? NaN : Number(e);
        function Ct(e, r, t, n, i, a, s) {
            if (y) return x(10, 0, 1, e, r, t, n, i, a, s);
            i = Er(i);
            try {
                u(!isNaN(i));
                var l = I.getStreamFromFD(n), d = o.mmap(l, e, i, r, t), m = d.ptr;
                return (f(), R)[a >> 2] = d.allocated, (f(), h)[s >> 2] = m, 0;
            } catch (v) {
                if (typeof o > "u" || v.name !== "ErrnoError") throw v;
                return -v.errno;
            }
        }
        function It(e, r, t, n, i, a) {
            if (y) return x(11, 0, 1, e, r, t, n, i, a);
            a = Er(a);
            try {
                var s = I.getStreamFromFD(i);
                t & 2 && I.doMsync(e, s, r, n, a);
            } catch (l) {
                if (typeof o > "u" || l.name !== "ErrnoError") throw l;
                return -l.errno;
            }
        }
        var Oo = (e, r, t, n)=>{
            var i = (new Date()).getFullYear(), a = new Date(i, 0, 1), s = new Date(i, 6, 1), l = a.getTimezoneOffset(), d = s.getTimezoneOffset(), m = Math.max(l, d);
            (f(), h)[e >> 2] = m * 60, (f(), R)[r >> 2] = +(l != d);
            var v = (p)=>{
                var k = p >= 0 ? "-" : "+", S = Math.abs(p), N = String(Math.floor(S / 60)).padStart(2, "0"), j = String(S % 60).padStart(2, "0");
                return `UTC${k}${N}${j}`;
            }, g = v(l), _ = v(d);
            u(g), u(_), u(ue(g) <= 16, `timezone name truncated to fit in TZNAME_MAX (${g})`), u(ue(_) <= 16, `timezone name truncated to fit in TZNAME_MAX (${_})`), d < l ? (ee(g, t, 17), ee(_, n, 17)) : (ee(g, n, 17), ee(_, t, 17));
        }, Mt = ()=>performance.timeOrigin + performance.now(), Wo = ()=>Date.now(), xo = (e)=>e >= 0 && e <= 3;
        function Lo(e, r, t) {
            if (!xo(e)) return 28;
            var n;
            e === 0 ? n = Wo() : n = Mt();
            var i = Math.round(n * 1e3 * 1e3);
            return (f(), O)[t >> 3] = BigInt(i), 0;
        }
        var qe = [], Uo = (e, r)=>{
            u(Array.isArray(qe)), u(r % 16 == 0), qe.length = 0;
            for(var t; t = (f(), H)[e++];){
                var n = String.fromCharCode(t), i = [
                    "d",
                    "f",
                    "i",
                    "p"
                ];
                i.push("j"), u(i.includes(n), `Invalid character ${t}("${n}") in readEmAsmArgs! Use only [${i}], and do not specify "v" for void return argument.`);
                var a = t != 105;
                a &= t != 112, r += a && r % 8 ? 4 : 0, qe.push(t == 112 ? (f(), h)[r >> 2] : t == 106 ? (f(), O)[r >> 3] : t == 105 ? (f(), R)[r >> 2] : (f(), Re)[r >> 3]), r += a ? 8 : 4;
            }
            return qe;
        }, $o = (e, r, t)=>{
            var n = Uo(r, t);
            return u(br.hasOwnProperty(e), `No EM_ASM constant found at address ${e}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`), br[e](...n);
        }, Bo = (e, r, t)=>$o(e, r, t), jo = ()=>{
            T || L || ie("Blocking on the main thread is very dangerous, see https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread");
        }, zo = (e, r)=>b(Z(e, r)), Dt = ()=>{
            ge += 1;
        }, Ho = ()=>{
            throw Dt(), "unwind";
        }, Rt = ()=>2147483648, Vo = ()=>Rt(), Go = ()=>T ? J("os").cpus().length : navigator.hardwareConcurrency, fe = {}, Nt = (e)=>{
            var r = ue(e) + 1, t = Ze(r);
            return t && ee(e, t, r), t;
        }, Ye = (e)=>{
            var r;
            if (r = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(e)) return +r[1];
            if (r = /\bwasm-function\[(\d+)\]:(\d+)/.exec(e)) ie("legacy backtrace format detected, this version of v8 is no longer supported by the emscripten backtrace mechanism");
            else if (r = /:(\d+):\d+(?:\)|$)/.exec(e)) return 2147483648 | +r[1];
            return 0;
        }, Ot = (e)=>{
            for (var r of e){
                var t = Ye(r);
                t && (fe[t] = r);
            }
        }, Wt = ()=>new Error().stack.toString(), Ko = ()=>{
            var e = Wt().split(`
`);
            return e[0] == "Error" && e.shift(), Ot(e), fe.last_addr = Ye(e[3]), fe.last_stack = e, fe.last_addr;
        }, Je = (e)=>{
            var r = fe[e];
            if (!r) return 0;
            var t, n;
            if (n = /^\s+at .*\.wasm\.(.*) \(.*\)$/.exec(r)) t = n[1];
            else if (n = /^\s+at (.*) \(.*\)$/.exec(r)) t = n[1];
            else if (n = /^(.+?)@/.exec(r)) t = n[1];
            else return 0;
            return re(Je.ret ?? 0), Je.ret = Nt(t), Je.ret;
        }, Xo = (e)=>{
            var r = K.buffer.byteLength, t = (e - r + 65535) / 65536 | 0;
            try {
                return K.grow(t), je(), 1;
            } catch (n) {
                b(`growMemory: Attempted to grow heap from ${r} bytes to ${e} bytes, but got error: ${n}`);
            }
        }, qo = (e)=>{
            var r = (f(), H).length;
            if (e >>>= 0, e <= r) return !1;
            var t = Rt();
            if (e > t) return b(`Cannot enlarge memory, requested ${e} bytes, but the limit is ${t} bytes!`), !1;
            for(var n = 1; n <= 4; n *= 2){
                var i = r * (1 + 0.2 / n);
                i = Math.min(i, e + 100663296);
                var a = Math.min(t, st(Math.max(e, i), 65536)), s = Xo(a);
                if (s) return !0;
            }
            return b(`Failed to grow the heap from ${r} bytes to ${a} bytes, not enough memory!`), !1;
        }, Yo = (e, r, t)=>{
            var n;
            fe.last_addr == e ? n = fe.last_stack : (n = Wt().split(`
`), n[0] == "Error" && n.shift(), Ot(n));
            for(var i = 3; n[i] && Ye(n[i]) != e;)++i;
            for(var a = 0; a < t && n[a + i]; ++a)(f(), R)[r + a * 4 >> 2] = Ye(n[a + i]);
            return a;
        }, wr = {}, Jo = ()=>er || "./this.program", Oe = ()=>{
            if (!Oe.strings) {
                var e = (typeof navigator == "object" && navigator.language || "C").replace("-", "_") + ".UTF-8", r = {
                    USER: "web_user",
                    LOGNAME: "web_user",
                    PATH: "/",
                    PWD: "/",
                    HOME: "/home/web_user",
                    LANG: e,
                    _: Jo()
                };
                for(var t in wr)wr[t] === void 0 ? delete r[t] : r[t] = wr[t];
                var n = [];
                for(var t in r)n.push(`${t}=${r[t]}`);
                Oe.strings = n;
            }
            return Oe.strings;
        };
        function xt(e, r) {
            if (y) return x(12, 0, 1, e, r);
            var t = 0, n = 0;
            for (var i of Oe()){
                var a = r + t;
                (f(), h)[e + n >> 2] = a, t += ee(i, a, 1 / 0) + 1, n += 4;
            }
            return 0;
        }
        function Lt(e, r) {
            if (y) return x(13, 0, 1, e, r);
            var t = Oe();
            (f(), h)[e >> 2] = t.length;
            var n = 0;
            for (var i of t)n += ue(i) + 1;
            return (f(), h)[r >> 2] = n, 0;
        }
        function Ut(e) {
            if (y) return x(14, 0, 1, e);
            try {
                var r = I.getStreamFromFD(e);
                return o.close(r), 0;
            } catch (t) {
                if (typeof o > "u" || t.name !== "ErrnoError") throw t;
                return t.errno;
            }
        }
        var Zo = (e, r, t, n)=>{
            for(var i = 0, a = 0; a < t; a++){
                var s = (f(), h)[r >> 2], l = (f(), h)[r + 4 >> 2];
                r += 8;
                var d = o.read(e, (f(), U), s, l, n);
                if (d < 0) return -1;
                if (i += d, d < l) break;
            }
            return i;
        };
        function $t(e, r, t, n) {
            if (y) return x(15, 0, 1, e, r, t, n);
            try {
                var i = I.getStreamFromFD(e), a = Zo(i, r, t);
                return (f(), h)[n >> 2] = a, 0;
            } catch (s) {
                if (typeof o > "u" || s.name !== "ErrnoError") throw s;
                return s.errno;
            }
        }
        function Bt(e, r, t, n) {
            if (y) return x(16, 0, 1, e, r, t, n);
            r = Er(r);
            try {
                if (isNaN(r)) return 61;
                var i = I.getStreamFromFD(e);
                return o.llseek(i, r, t), (f(), O)[n >> 3] = BigInt(i.position), i.getdents && r === 0 && t === 0 && (i.getdents = null), 0;
            } catch (a) {
                if (typeof o > "u" || a.name !== "ErrnoError") throw a;
                return a.errno;
            }
        }
        var Qo = (e, r, t, n)=>{
            for(var i = 0, a = 0; a < t; a++){
                var s = (f(), h)[r >> 2], l = (f(), h)[r + 4 >> 2];
                r += 8;
                var d = o.write(e, (f(), U), s, l, n);
                if (d < 0) return -1;
                if (i += d, d < l) break;
            }
            return i;
        };
        function jt(e, r, t, n) {
            if (y) return x(17, 0, 1, e, r, t, n);
            try {
                var i = I.getStreamFromFD(e), a = Qo(i, r, t);
                return (f(), h)[n >> 2] = a, 0;
            } catch (s) {
                if (typeof o > "u" || s.name !== "ErrnoError") throw s;
                return s.errno;
            }
        }
        function ei(e, r) {
            try {
                return _r((f(), H).subarray(e, e + r)), 0;
            } catch (t) {
                if (typeof o > "u" || t.name !== "ErrnoError") throw t;
                return t.errno;
            }
        }
        var ri = ()=>{
            u(ge > 0), ge -= 1;
        }, q = {
            instrumentWasmImports (e) {
                u("Suspending" in WebAssembly, "JSPI not supported by current environment. Perhaps it needs to be enabled via flags?");
                var r = /^(invoke_.*|__asyncjs__.*)$/;
                for (let [t, n] of Object.entries(e))typeof n == "function" && (n.isAsync || r.test(t)) && (e[t] = n = new WebAssembly.Suspending(n));
            },
            instrumentFunction (e) {
                var r = (...t)=>e(...t);
                return r = St(`__asyncify_wrapper_${e.name}`, r), r;
            },
            instrumentWasmExports (e) {
                var r = /^(solve_model|validate_model|main|__main_argc_argv)$/;
                q.asyncExports = new Set();
                var t = {};
                for (let [i, a] of Object.entries(e))if (typeof a == "function") {
                    r.test(i) && (q.asyncExports.add(a), a = q.makeAsyncFunction(a));
                    var n = q.instrumentFunction(a);
                    t[i] = n;
                } else t[i] = a;
                return t;
            },
            asyncExports: null,
            isAsyncExport (e) {
                return q.asyncExports?.has(e);
            },
            handleAsync: async (e)=>{
                Dt();
                try {
                    return await e();
                } finally{
                    ri();
                }
            },
            handleSleep: (e)=>q.handleAsync(()=>new Promise(e)),
            makeAsyncFunction (e) {
                return WebAssembly.promising(e);
            }
        }, ti = (e)=>{
            var r = c["_" + e];
            return u(r, "Cannot call unknown function " + e + ", make sure it is exported"), r;
        }, ni = (e, r)=>{
            u(e.length >= 0, "writeArrayToMemory array must have a length (should be an array or typed array)"), (f(), U).set(e, r);
        }, oi = (e)=>{
            var r = ue(e) + 1, t = dr(r);
            return ee(e, t, r), t;
        }, zt = (e, r, t, n, i)=>{
            var a = {
                string: (S)=>{
                    var N = 0;
                    return S != null && S !== 0 && (N = oi(S)), N;
                },
                array: (S)=>{
                    var N = dr(S.length);
                    return ni(S, N), N;
                }
            };
            function s(S) {
                return r === "string" ? Z(S) : r === "boolean" ? !!S : S;
            }
            var l = ti(e), d = [], m = 0;
            if (u(r !== "array", 'Return type should not be "array".'), n) for(var v = 0; v < n.length; v++){
                var g = a[t[v]];
                g ? (m === 0 && (m = Jr()), d[v] = g(n[v])) : d[v] = n[v];
            }
            var _ = l(...d);
            function p(S) {
                return m !== 0 && lr(m), s(S);
            }
            var k = i?.async;
            return k ? _.then(p) : (_ = p(_), _);
        }, ii = (e, r, t, n)=>(...i)=>zt(e, r, t, i, n), ai = (...e)=>Nt(...e);
        w.init(), o.createPreloadedFile = Bn, o.preloadFile = ut, o.staticInit(), u(Q.length === 10);
        {
            if (hn(), c.noExitRuntime && (mr = c.noExitRuntime), c.preloadPlugins && (ct = c.preloadPlugins), c.print && (ne = c.print), c.printErr && (b = c.printErr), c.wasmBinary && (Ie = c.wasmBinary), ci(), c.arguments && c.arguments, c.thisProgram && (er = c.thisProgram), u(typeof c.memoryInitializerPrefixURL > "u", "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead"), u(typeof c.pthreadMainPrefixURL > "u", "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead"), u(typeof c.cdInitializerPrefixURL > "u", "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead"), u(typeof c.filePackagePrefixURL > "u", "Module.filePackagePrefixURL option was removed, use Module.locateFile instead"), u(typeof c.read > "u", "Module.read option was removed"), u(typeof c.readAsync > "u", "Module.readAsync option was removed (modify readAsync in JS)"), u(typeof c.readBinary > "u", "Module.readBinary option was removed (modify readBinary in JS)"), u(typeof c.setWindowTitle > "u", "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)"), u(typeof c.TOTAL_MEMORY > "u", "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY"), u(typeof c.ENVIRONMENT > "u", "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)"), u(typeof c.STACK_SIZE > "u", "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time"), c.preInit) for(typeof c.preInit == "function" && (c.preInit = [
                c.preInit
            ]); c.preInit.length > 0;)c.preInit.shift()();
            Be("preInit");
        }
        c.ccall = zt, c.cwrap = ii, c.UTF8ToString = Z, c.stringToUTF8 = ee, c.allocateUTF8 = ai;
        var si = [
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
        si.forEach(_n);
        var li = [
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
        li.forEach(Rr);
        var di = [
            cr,
            Zr,
            ot,
            ft,
            mt,
            _t,
            vt,
            ht,
            pt,
            gt,
            Ct,
            It,
            xt,
            Lt,
            Ut,
            $t,
            Bt,
            jt
        ];
        function ci() {
            fn("fetchSettings");
        }
        var br = {
            540644: ()=>typeof wasmOffsetConverter < "u"
        };
        function Ht() {
            return typeof wasmOffsetConverter < "u";
        }
        Ht.sig = "i";
        var Vt = A("___getTypeName"), Gt = A("__embind_initialize_bindings");
        c._get_cp_model_schema = A("_get_cp_model_schema"), c._get_sat_parameters_schema = A("_get_sat_parameters_schema"), c._solve_model = A("_solve_model");
        var Ze = c._malloc = A("_malloc");
        c._free_buffer = A("_free_buffer");
        var re = c._free = A("_free");
        c._interrupt_solve = A("_interrupt_solve"), c._validate_model = A("_validate_model");
        var ke = A("_pthread_self"), Sr = A("_fflush"), Kt = A("_strerror"), Xt = A("_emscripten_builtin_memalign"), kr = A("__emscripten_thread_init"), qt = A("__emscripten_thread_crashed"), Tr = A("_emscripten_stack_get_end"), Yt = A("__emscripten_run_js_on_main_thread"), Jt = A("__emscripten_thread_free_data"), Fr = A("__emscripten_thread_exit"), Zt = A("__emscripten_check_mailbox"), Qt = A("_emscripten_stack_init"), en = A("_emscripten_stack_set_limits"), rn = A("__emscripten_stack_restore"), tn = A("__emscripten_stack_alloc"), Ar = A("_emscripten_stack_get_current"), nn = A("wasmTable");
        function ui(e) {
            u(typeof e.__getTypeName < "u", "missing Wasm export: __getTypeName"), u(typeof e._embind_initialize_bindings < "u", "missing Wasm export: _embind_initialize_bindings"), u(typeof e.get_cp_model_schema < "u", "missing Wasm export: get_cp_model_schema"), u(typeof e.get_sat_parameters_schema < "u", "missing Wasm export: get_sat_parameters_schema"), u(typeof e.solve_model < "u", "missing Wasm export: solve_model"), u(typeof e.malloc < "u", "missing Wasm export: malloc"), u(typeof e.free_buffer < "u", "missing Wasm export: free_buffer"), u(typeof e.free < "u", "missing Wasm export: free"), u(typeof e.interrupt_solve < "u", "missing Wasm export: interrupt_solve"), u(typeof e.validate_model < "u", "missing Wasm export: validate_model"), u(typeof e.pthread_self < "u", "missing Wasm export: pthread_self"), u(typeof e.fflush < "u", "missing Wasm export: fflush"), u(typeof e.strerror < "u", "missing Wasm export: strerror"), u(typeof e._emscripten_tls_init < "u", "missing Wasm export: _emscripten_tls_init"), u(typeof e.emscripten_builtin_memalign < "u", "missing Wasm export: emscripten_builtin_memalign"), u(typeof e._emscripten_thread_init < "u", "missing Wasm export: _emscripten_thread_init"), u(typeof e._emscripten_thread_crashed < "u", "missing Wasm export: _emscripten_thread_crashed"), u(typeof e.emscripten_stack_get_end < "u", "missing Wasm export: emscripten_stack_get_end"), u(typeof e.emscripten_stack_get_base < "u", "missing Wasm export: emscripten_stack_get_base"), u(typeof e._emscripten_run_js_on_main_thread < "u", "missing Wasm export: _emscripten_run_js_on_main_thread"), u(typeof e._emscripten_thread_free_data < "u", "missing Wasm export: _emscripten_thread_free_data"), u(typeof e._emscripten_thread_exit < "u", "missing Wasm export: _emscripten_thread_exit"), u(typeof e._emscripten_check_mailbox < "u", "missing Wasm export: _emscripten_check_mailbox"), u(typeof e.emscripten_stack_init < "u", "missing Wasm export: emscripten_stack_init"), u(typeof e.emscripten_stack_set_limits < "u", "missing Wasm export: emscripten_stack_set_limits"), u(typeof e.emscripten_stack_get_free < "u", "missing Wasm export: emscripten_stack_get_free"), u(typeof e._emscripten_stack_restore < "u", "missing Wasm export: _emscripten_stack_restore"), u(typeof e._emscripten_stack_alloc < "u", "missing Wasm export: _emscripten_stack_alloc"), u(typeof e.emscripten_stack_get_current < "u", "missing Wasm export: emscripten_stack_get_current"), u(typeof e.__indirect_function_table < "u", "missing Wasm export: __indirect_function_table"), Vt = W("__getTypeName", 1), Gt = W("_embind_initialize_bindings", 0), c._get_cp_model_schema = W("get_cp_model_schema", 0), c._get_sat_parameters_schema = W("get_sat_parameters_schema", 0), c._solve_model = W("solve_model", 5), Ze = c._malloc = W("malloc", 1), c._free_buffer = W("free_buffer", 1), re = c._free = W("free", 1), c._interrupt_solve = W("interrupt_solve", 0), c._validate_model = W("validate_model", 3), ke = W("pthread_self", 0), Sr = W("fflush", 1), Kt = W("strerror", 1), Xt = W("emscripten_builtin_memalign", 2), kr = W("_emscripten_thread_init", 6), qt = W("_emscripten_thread_crashed", 0), Tr = e.emscripten_stack_get_end, e.emscripten_stack_get_base, Yt = W("_emscripten_run_js_on_main_thread", 5), Jt = W("_emscripten_thread_free_data", 1), Fr = W("_emscripten_thread_exit", 1), Zt = W("_emscripten_check_mailbox", 0), Qt = e.emscripten_stack_init, en = e.emscripten_stack_set_limits, e.emscripten_stack_get_free, rn = e._emscripten_stack_restore, tn = e._emscripten_stack_alloc, Ar = e.emscripten_stack_get_current, nn = e.__indirect_function_table;
        }
        var Te;
        function fi() {
            Te = {
                HaveOffsetConverter: Ht,
                __assert_fail: Pn,
                __cxa_throw: In,
                __pthread_create_js: it,
                __syscall_fcntl64: ft,
                __syscall_fstat64: mt,
                __syscall_ioctl: _t,
                __syscall_lstat64: vt,
                __syscall_newfstatat: ht,
                __syscall_openat: pt,
                __syscall_stat64: gt,
                _abort_js: jn,
                _embind_register_bigint: Vn,
                _embind_register_bool: Gn,
                _embind_register_emval: qn,
                _embind_register_float: Jn,
                _embind_register_function: vo,
                _embind_register_integer: ho,
                _embind_register_memory_view: po,
                _embind_register_std_string: go,
                _embind_register_std_wstring: To,
                _embind_register_void: Fo,
                _emscripten_init_main_thread_js: Ao,
                _emscripten_notify_mailbox_postmessage: Co,
                _emscripten_receive_on_main_thread_js: Io,
                _emscripten_thread_cleanup: Mo,
                _emscripten_thread_mailbox_await: yr,
                _emscripten_thread_set_strongref: Do,
                _mmap_js: Ct,
                _munmap_js: It,
                _tzset_js: Oo,
                clock_time_get: Lo,
                emscripten_asm_const_int: Bo,
                emscripten_check_blocking_allowed: jo,
                emscripten_errn: zo,
                emscripten_exit_with_live_runtime: Ho,
                emscripten_get_heap_max: Vo,
                emscripten_get_now: Mt,
                emscripten_num_logical_cores: Go,
                emscripten_pc_get_function: Je,
                emscripten_resize_heap: qo,
                emscripten_stack_snapshot: Ko,
                emscripten_stack_unwind_buffer: Yo,
                environ_get: xt,
                environ_sizes_get: Lt,
                exit: ur,
                fd_close: Ut,
                fd_read: $t,
                fd_seek: Bt,
                fd_write: jt,
                memory: K,
                proc_exit: cr,
                random_get: ei
            };
        }
        var on;
        function mi() {
            u(!y), Qt(), Dr();
        }
        function Qe() {
            if (de > 0) {
                Ne = Qe;
                return;
            }
            if (y) {
                or?.(c), Ur();
                return;
            }
            if (mi(), pn(), de > 0) {
                Ne = Qe;
                return;
            }
            async function e() {
                u(!on), on = !0, c.calledRun = !0, !_e && (Ur(), or?.(c), c.onRuntimeInitialized?.(), Be("onRuntimeInitialized"), u(!c._main, 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]'), gn());
            }
            c.setStatus ? (c.setStatus("Running..."), setTimeout(()=>{
                setTimeout(()=>c.setStatus(""), 1), e();
            }, 1)) : e(), Me();
        }
        function _i() {
            var e = ne, r = b, t = !1;
            ne = b = (d)=>{
                t = !0;
            };
            try {
                Sr(0);
                for (var n of [
                    "stdout",
                    "stderr"
                ]){
                    var i = o.analyzePath("/dev/" + n);
                    if (!i) return;
                    var a = i.object, s = a.rdev, l = ae.ttys[s];
                    l?.output?.length && (t = !0);
                }
            } catch  {}
            ne = e, b = r, t && ie("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
        }
        var te;
        y || (te = await Br(), Qe()), he ? P = c : P = new Promise((e, r)=>{
            or = e, ir = r;
        });
        for (const e of Object.keys(c))e in M || Object.defineProperty(M, e, {
            configurable: !0,
            get () {
                C(`Access to module property ('${e}') is no longer possible via the module constructor argument; Instead, use the result of the module constructor.`);
            }
        });
        return P;
    }
    var ln = globalThis.self?.name?.startsWith("em-pthread"), vi = globalThis.process?.versions?.node && globalThis.process?.type != "renderer";
    vi && (ln = (await Promise.resolve().then(function() {
        return an;
    })).workerData === "em-pthread");
    ln && sn();
    async function hi() {
        return await sn({});
    }
    const Ae = self;
    let D = null;
    const pi = hi().then((M)=>(D = M, Ae.postMessage({
            type: "ready"
        }), M)).catch((M)=>{
        throw console.error("[cpsat_worker] cpSatModule init failed:", M), Ae.postMessage({
            type: "error",
            id: 0,
            error: String(M)
        }), M;
    }), dn = (M, P)=>new DataView(M, P, 4).getUint32(0, !0), Pr = (M)=>{
        if (!D || !M?.length) return 0;
        const P = D._malloc(M.length);
        return D.HEAPU8.set(M, P), P;
    };
    async function gi(M, P) {
        if (!D) throw new Error("Module not initialized.");
        const c = D._malloc(4), B = Pr(M), L = Pr(P ?? null);
        let T = 0;
        try {
            T = await D.ccall("solve_model", "number", [
                "number",
                "number",
                "number",
                "number",
                "number"
            ], [
                B,
                M.length,
                L,
                P ? P.length : 0,
                c
            ], {
                async: !0
            });
        } finally{
            B && D._free(B), L && D._free(L);
        }
        const me = dn(D.HEAPU8.buffer, c);
        if (D._free(c), !T || me === 0) return T && D._free_buffer(T), new Uint8Array();
        const y = D.HEAPU8.slice(T, T + me);
        return D._free_buffer(T), y;
    }
    async function yi(M) {
        if (!D) throw new Error("Module not initialized.");
        const P = D._malloc(4), c = Pr(M);
        let B = 0;
        try {
            B = D.ccall("validate_model", "number", [
                "number",
                "number",
                "number"
            ], [
                c,
                M.length,
                P
            ]);
        } finally{
            c && D._free(c);
        }
        const L = dn(D.HEAPU8.buffer, P);
        if (D._free(P), !B || L === 0) return B && D._free_buffer(B), {
            ok: !0,
            message: ""
        };
        const T = D.HEAPU8.slice(B, B + L);
        return D._free_buffer(B), {
            ok: !1,
            message: new TextDecoder().decode(T)
        };
    }
    Ae.onmessage = async (M)=>{
        const P = M.data;
        try {
            if (await pi, !D) throw new Error("Module not initialized.");
            if (P.type === "validate") {
                const c = await yi(P.modelBytes);
                Ae.postMessage({
                    type: "validateResult",
                    id: P.id,
                    ok: c.ok,
                    message: c.message
                });
                return;
            }
            if (P.type === "solve") {
                const c = await gi(P.modelBytes, P.paramsBytes);
                Ae.postMessage({
                    type: "solveResult",
                    id: P.id,
                    bytes: c
                });
                return;
            }
        } catch (c) {
            console.error("[cpsat_worker] request failed", P?.type, c), Ae.postMessage({
                type: "error",
                id: P?.id ?? 0,
                error: String(c)
            });
        }
    };
})();
