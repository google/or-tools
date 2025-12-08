declare module '*.wasm?url' {
  const src: string;
  export default src;
}

declare module '*.d.ts?url' {
  const src: string;
  export default src;
}

declare module '*.js?worker&url' {
  const src: string;
  export default src;
}

declare module './cpsat_worker?worker' {
  type WorkerClass = new () => Worker;
  const WorkerCtor: WorkerClass;
  export default WorkerCtor;
}
