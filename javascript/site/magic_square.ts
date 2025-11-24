(() => {
  type MagicSquareExpr = {
    vars: number[];
    coeffs: number[];
    offset: number;
  };

  type MagicSquareModel = {
    name: string;
    variables: Array<{ name: string; domain: number[] }>;
    constraints: Array<
      | { name: string; all_diff: { exprs: MagicSquareExpr[] } }
      | {
          name: string;
          linear: { vars: number[]; coeffs: number[]; domain: number[] };
        }
    >;
  };

  const statusEl = document.getElementById('status') as HTMLPreElement | null;
  const solutionGrid = document.getElementById('solution-grid') as HTMLElement | null;
  const sizeInput = document.getElementById('size') as HTMLInputElement | null;
  const workerInput = document.getElementById('workers') as HTMLInputElement | null;
  const runButton = document.getElementById('run') as HTMLButtonElement | null;
  const readyIndicator = document.getElementById('ready-indicator') as HTMLElement | null;

  function append(text: string) {
    if (statusEl) {
      statusEl.textContent += `${text}\n`;
    }
  }

  function showSolutionMessage(message: string) {
    if (!solutionGrid) return;
    solutionGrid.textContent = message;
    solutionGrid.style.removeProperty('gridTemplateColumns');
  }

  function renderSolution(size: number, values: Array<number | string>) {
    if (!solutionGrid) return;
    if (!values.length) {
      showSolutionMessage('Solver returned no solution.');
      return;
    }

    const normalize = (value: number | string) => {
      if (typeof value === 'number') return value;
      const parsed = Number.parseInt(value, 10);
      return Number.isFinite(parsed) ? parsed : 0;
    };

    const parsedValues = values.map(normalize);
    solutionGrid.innerHTML = '';
    solutionGrid.style.gridTemplateColumns = `repeat(${size}, minmax(2.5rem, auto))`;
    for (let r = 0; r < size; ++r) {
      for (let c = 0; c < size; ++c) {
        const idx = r * size + c;
        const cell = document.createElement('div');
        cell.className = 'cell';
        cell.textContent = String(parsedValues[idx] ?? '?');
        solutionGrid.appendChild(cell);
      }
    }
  }

  function isRecord(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null;
  }

  function buildMagicSquareModel(size: number): MagicSquareModel {
    const numCells = size * size;
    const variables: MagicSquareModel['variables'] = [];
    const domain: [number, number] = [1, numCells];
    for (let r = 0; r < size; ++r) {
      for (let c = 0; c < size; ++c) {
        variables.push({
          name: `cell_${r}_${c}`,
          domain: [...domain],
        });
      }
    }

    const constraints: MagicSquareModel['constraints'] = [];
    const allDiffExprs: MagicSquareExpr[] = [];
    for (let i = 0; i < numCells; ++i) {
      allDiffExprs.push({ vars: [i], coeffs: [1], offset: 0 });
    }
    constraints.push({
      name: 'all_diff',
      all_diff: { exprs: allDiffExprs },
    });

    const target = (size * (size * size + 1)) / 2;
    const addLinearConstraint = (name: string, vars: number[]) => {
      constraints.push({
        name,
        linear: {
          vars,
          coeffs: Array(vars.length).fill(1),
          domain: [target, target],
        },
      });
    };

    for (let r = 0; r < size; ++r) {
      const vars: number[] = [];
      for (let c = 0; c < size; ++c) {
        vars.push(r * size + c);
      }
      addLinearConstraint(`row_${r}`, vars);
    }

    for (let c = 0; c < size; ++c) {
      const vars: number[] = [];
      for (let r = 0; r < size; ++r) {
        vars.push(r * size + c);
      }
      addLinearConstraint(`col_${c}`, vars);
    }

    const diagMain: number[] = [];
    const diagAnti: number[] = [];
    for (let i = 0; i < size; ++i) {
      diagMain.push(i * size + i);
      diagAnti.push(i * size + (size - 1 - i));
    }
    addLinearConstraint('diag_main', diagMain);
    addLinearConstraint('diag_anti', diagAnti);

    return {
      name: `magic_square_${size}`,
      variables,
      constraints,
    };
  }

  async function runMagicSquare() {
    if (!sizeInput || !workerInput) {
      append('Missing configuration inputs.');
      return;
    }

    const size = Math.max(1, Number.parseInt(sizeInput.value, 10) || 1);
    const workers = Number.parseInt(workerInput.value, 10) || 0;
    append(`Building model (size=${size})…`);
    showSolutionMessage('Solving…');

    let model: CpSatModelInstance;
    try {
      model = await CpSat.createModel(buildMagicSquareModel(size));
    } catch (err) {
      append(`Model build failed: ${(err as Error).message}`);
      return;
    }

    const validation = await CpSat.validate(model);
    if (!validation.ok) {
      append(`Model invalid: ${validation.message}`);
      return;
    }

    const params: Record<string, unknown> = {};
    if (workers > 0) {
      params.num_search_workers = workers;
    }

    append('Solving…');
    try {
      const result = await CpSat.solve(model, params);
      if (!result.response || !statusEl) {
        append('Solver returned no response.');
        showSolutionMessage('Solver returned no response.');
        return;
      }
      statusEl.textContent = JSON.stringify(result.response, null, 2);

      if (!isRecord(result.response) || !Array.isArray(result.response.solution)) {
        showSolutionMessage('No solution entries returned.');
        return;
      }
      renderSolution(size, result.response.solution as Array<number | string>);
    } catch (err) {
      append(`Solve failed: ${(err as Error).message}`);
      showSolutionMessage('Solve failed.');
    }
  }

  if (runButton) {
    runButton.addEventListener('click', () => {
      void runMagicSquare();
    });
  }

  void CpSat.loadModule()
    .then(() => {
      if (readyIndicator) {
        readyIndicator.textContent = 'Module ready.';
      }
    })
    .catch((err) => {
      if (readyIndicator) {
        readyIndicator.textContent = 'Module failed to load.';
      }
      append(String(err));
    });
})();
