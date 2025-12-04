import { CpSat } from 'ortools-cpsat-wasm';

const statusEl = document.getElementById('status') as HTMLElement | null;
const cpEl = document.getElementById('cp-schema') as HTMLTextAreaElement | null;
const satEl = document.getElementById('sat-schema') as HTMLTextAreaElement | null;
const button = document.getElementById('load-schemas') as HTMLButtonElement | null;

async function loadSchemas() {
  if (!statusEl || !button || !cpEl || !satEl) {
    return;
  }
  statusEl.textContent = 'Fetching embedded schemas…';
  button.disabled = true;
  try {
    const schemas = await CpSat.getSchemas();
    cpEl.value = schemas.cp_model;
    satEl.value = schemas.sat_parameters;
    statusEl.textContent = 'Schemas loaded.';
  } catch (err) {
    statusEl.textContent = `Failed to load schemas: ${(err as Error).message}`;
    button.disabled = false;
  }
}

if (button) {
  button.addEventListener('click', () => {
    void loadSchemas();
  });
}

void CpSat.loadModule()
  .then(() => {
    if (statusEl) {
      statusEl.textContent = 'Module ready.';
    }
    if (button) {
      button.disabled = false;
    }
  })
  .catch((err) => {
    if (statusEl) {
      statusEl.textContent = `Module failed to load: ${(err as Error).message}`;
    }
  });
