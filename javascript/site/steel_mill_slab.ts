import { CpSat, type CpSatModelInstance } from 'ortools-cpsat-wasm';

type SolverMethod = 'sat' | 'sat_table' | 'sat_column';

type Order = {
  width: number;
  color: number;
};

type ProblemData = {
  id: number;
  numSlabs: number;
  capacities: number[];
  numColors: number;
  orders: Order[];
};

const PROBLEMS: ProblemData[] = [
  {
    id: 0,
    numSlabs: 111,
    capacities: [
      0, 12, 14, 17, 18, 19, 20, 23, 24, 25, 26, 27, 28, 29, 30, 32, 35, 39, 42, 43, 44,
    ],
    numColors: 88,
    orders: [
      { width: 4, color: 1 },
      { width: 22, color: 2 },
      { width: 9, color: 3 },
      { width: 5, color: 4 },
      { width: 8, color: 5 },
      { width: 3, color: 6 },
      { width: 3, color: 4 },
      { width: 4, color: 7 },
      { width: 7, color: 4 },
      { width: 7, color: 8 },
      { width: 3, color: 6 },
      { width: 2, color: 6 },
      { width: 2, color: 4 },
      { width: 8, color: 9 },
      { width: 5, color: 10 },
      { width: 7, color: 11 },
      { width: 4, color: 7 },
      { width: 7, color: 11 },
      { width: 5, color: 10 },
      { width: 7, color: 11 },
      { width: 8, color: 9 },
      { width: 3, color: 1 },
      { width: 25, color: 12 },
      { width: 14, color: 13 },
      { width: 3, color: 6 },
      { width: 22, color: 14 },
      { width: 19, color: 15 },
      { width: 19, color: 15 },
      { width: 22, color: 16 },
      { width: 22, color: 17 },
      { width: 22, color: 18 },
      { width: 20, color: 19 },
      { width: 22, color: 20 },
      { width: 5, color: 21 },
      { width: 4, color: 22 },
      { width: 10, color: 23 },
      { width: 26, color: 24 },
      { width: 17, color: 25 },
      { width: 20, color: 26 },
      { width: 16, color: 27 },
      { width: 10, color: 28 },
      { width: 19, color: 29 },
      { width: 10, color: 30 },
      { width: 10, color: 31 },
      { width: 23, color: 32 },
      { width: 22, color: 33 },
      { width: 26, color: 34 },
      { width: 27, color: 35 },
      { width: 22, color: 36 },
      { width: 27, color: 37 },
      { width: 22, color: 38 },
      { width: 22, color: 39 },
      { width: 13, color: 40 },
      { width: 14, color: 41 },
      { width: 16, color: 27 },
      { width: 26, color: 34 },
      { width: 26, color: 42 },
      { width: 27, color: 35 },
      { width: 22, color: 36 },
      { width: 20, color: 43 },
      { width: 26, color: 24 },
      { width: 22, color: 44 },
      { width: 13, color: 45 },
      { width: 19, color: 46 },
      { width: 20, color: 47 },
      { width: 16, color: 48 },
      { width: 15, color: 49 },
      { width: 17, color: 50 },
      { width: 10, color: 28 },
      { width: 20, color: 51 },
      { width: 5, color: 52 },
      { width: 26, color: 24 },
      { width: 19, color: 53 },
      { width: 15, color: 54 },
      { width: 10, color: 55 },
      { width: 10, color: 56 },
      { width: 13, color: 57 },
      { width: 13, color: 58 },
      { width: 13, color: 59 },
      { width: 12, color: 60 },
      { width: 12, color: 61 },
      { width: 18, color: 62 },
      { width: 10, color: 63 },
      { width: 18, color: 64 },
      { width: 16, color: 65 },
      { width: 20, color: 66 },
      { width: 12, color: 67 },
      { width: 6, color: 68 },
      { width: 6, color: 68 },
      { width: 15, color: 69 },
      { width: 15, color: 70 },
      { width: 15, color: 70 },
      { width: 21, color: 71 },
      { width: 30, color: 72 },
      { width: 30, color: 73 },
      { width: 30, color: 74 },
      { width: 30, color: 75 },
      { width: 23, color: 76 },
      { width: 15, color: 77 },
      { width: 15, color: 78 },
      { width: 27, color: 79 },
      { width: 27, color: 80 },
      { width: 27, color: 81 },
      { width: 27, color: 82 },
      { width: 27, color: 83 },
      { width: 27, color: 84 },
      { width: 27, color: 79 },
      { width: 27, color: 85 },
      { width: 27, color: 86 },
      { width: 10, color: 87 },
      { width: 3, color: 88 },
    ],
  },
  {
    id: 1,
    numSlabs: 30,
    capacities: [0, 17, 44],
    numColors: 23,
    orders: [
      { width: 4, color: 1 },
      { width: 22, color: 2 },
      { width: 9, color: 3 },
      { width: 5, color: 4 },
      { width: 8, color: 5 },
      { width: 3, color: 6 },
      { width: 3, color: 4 },
      { width: 4, color: 7 },
      { width: 7, color: 4 },
      { width: 7, color: 8 },
      { width: 3, color: 6 },
      { width: 2, color: 6 },
      { width: 2, color: 4 },
      { width: 8, color: 9 },
      { width: 5, color: 10 },
      { width: 7, color: 11 },
      { width: 4, color: 7 },
      { width: 7, color: 11 },
      { width: 5, color: 10 },
      { width: 7, color: 11 },
      { width: 8, color: 9 },
      { width: 3, color: 1 },
      { width: 25, color: 12 },
      { width: 14, color: 13 },
      { width: 3, color: 6 },
      { width: 22, color: 14 },
      { width: 19, color: 15 },
      { width: 19, color: 15 },
      { width: 22, color: 16 },
      { width: 22, color: 17 },
      { width: 22, color: 18 },
      { width: 20, color: 19 },
      { width: 22, color: 20 },
      { width: 5, color: 21 },
      { width: 4, color: 22 },
      { width: 10, color: 23 },
    ],
  },
  {
    id: 2,
    numSlabs: 20,
    capacities: [0, 17, 44],
    numColors: 15,
    orders: [
      { width: 4, color: 1 },
      { width: 22, color: 2 },
      { width: 9, color: 3 },
      { width: 5, color: 4 },
      { width: 8, color: 5 },
      { width: 3, color: 6 },
      { width: 3, color: 4 },
      { width: 4, color: 7 },
      { width: 7, color: 4 },
      { width: 7, color: 8 },
      { width: 3, color: 6 },
      { width: 2, color: 6 },
      { width: 2, color: 4 },
      { width: 8, color: 9 },
      { width: 5, color: 10 },
      { width: 7, color: 11 },
      { width: 4, color: 7 },
      { width: 7, color: 11 },
      { width: 5, color: 10 },
      { width: 7, color: 11 },
      { width: 8, color: 9 },
      { width: 3, color: 1 },
      { width: 25, color: 12 },
      { width: 14, color: 13 },
      { width: 3, color: 6 },
      { width: 22, color: 14 },
      { width: 19, color: 15 },
      { width: 19, color: 15 },
    ],
  },
  {
    id: 3,
    numSlabs: 10,
    capacities: [0, 17, 44],
    numColors: 8,
    orders: [
      { width: 4, color: 1 },
      { width: 22, color: 2 },
      { width: 9, color: 3 },
      { width: 5, color: 4 },
      { width: 8, color: 5 },
      { width: 3, color: 6 },
      { width: 3, color: 4 },
      { width: 4, color: 7 },
      { width: 7, color: 4 },
      { width: 7, color: 8 },
      { width: 3, color: 6 },
    ],
  },
];

type CpModelInput = {
  name: string;
  variables: Array<{ name?: string; domain: [number, number] }>;
  constraints: Array<
    | {
        name?: string;
        linear: { vars: number[]; coeffs: number[]; domain: [number, number] };
      }
    | { name?: string; table: { vars: number[]; values: number[] } }
  >;
  objective?: {
    vars: number[];
    coeffs: number[];
    offset?: number;
    scaling_factor?: number;
  };
};

type ValidSlab = {
  assignment: number[];
  loss: number;
  load: number;
};

type ModelMetadata = {
  solver: SolverMethod;
  problem: ProblemData;
  assignByOrder?: number[][];
  loadIndices?: number[];
  lossIndices?: number[];
  colorIndices?: number[][];
  selectedIndices?: number[];
  validSlabs?: ValidSlab[];
};

type ModelBundle = {
  model: CpModelInput;
  metadata: ModelMetadata;
};

const statusEl = document.getElementById('status') as HTMLPreElement | null;
const summaryEl = document.getElementById('summary') as HTMLElement | null;
const readyIndicator = document.getElementById('ready-indicator') as HTMLElement | null;
const slabTable = document.getElementById('slab-table') as HTMLTableElement | null;
const solverSelect = document.getElementById('solver-method') as HTMLSelectElement | null;
const problemSelect = document.getElementById('problem-id') as HTMLSelectElement | null;
const breakSymCheckbox = document.getElementById('break-symmetries') as HTMLInputElement | null;
const workersInput = document.getElementById('workers') as HTMLInputElement | null;
const runButton = document.getElementById('run') as HTMLButtonElement | null;
const stopButton = document.getElementById('stop') as HTMLButtonElement | null;
const paramsInput = document.getElementById('params') as HTMLInputElement | null;
const workerBridgeToggle = document.getElementById('use-worker-bridge') as HTMLInputElement | null;
const maxWorkerCount = Math.max(1, navigator.hardwareConcurrency || 1);

const appendStatus = (text: string) => {
  if (!statusEl) return;
  statusEl.textContent += `${text}\n`;
};

const resetStatus = () => {
  if (statusEl) {
    statusEl.textContent = '';
  }
};

const setReadyIndicator = (text: string) => {
  if (readyIndicator) {
    readyIndicator.textContent = text;
  }
};

const applyWorkerBridgePreference = (enabled: boolean) => {
  if (workerBridgeToggle) {
    workerBridgeToggle.checked = enabled;
  }
  if (typeof CpSat?.setWorkerBridgeEnabled === 'function') {
    CpSat.setWorkerBridgeEnabled(enabled);
  }
};

if (workerBridgeToggle) {
  workerBridgeToggle.checked = true;
  workerBridgeToggle.addEventListener('change', () => {
    applyWorkerBridgePreference(workerBridgeToggle.checked);
  });
  applyWorkerBridgePreference(true);
}

if (workersInput) {
  workersInput.max = String(maxWorkerCount);
  workersInput.min = '1';
  workersInput.value = String(maxWorkerCount);
}

const shouldUseWorkerBridge = () => Boolean(workerBridgeToggle?.checked);

const toCamelCase = (value: string) =>
  value.replace(/_([a-z])/g, (_, letter) => letter.toUpperCase());

const parseParamsInput = (text: string | null): Record<string, unknown> => {
  if (!text) return {};
  return text
    .split(',')
    .map((token) => token.trim())
    .filter(Boolean)
    .reduce<Record<string, unknown>>((params, pair) => {
      const [key, value] = pair.split(':').map((part) => part.trim());
      if (!key || value === undefined) return params;
      const normalizedKey = toCamelCase(key);
      if (/^(true|false)$/i.test(value)) {
        params[normalizedKey] = value.toLowerCase() === 'true';
      } else if (!Number.isNaN(Number(value))) {
        params[normalizedKey] = Number(value);
      } else {
        params[normalizedKey] = value;
      }
      return params;
    }, {});
};

const createProblemSelector = () => {
  if (!problemSelect) return;
  PROBLEMS.forEach((problem) => {
    const option = document.createElement('option');
    option.value = String(problem.id);
    option.textContent = `Problem ${problem.id} (${problem.numSlabs} slabs, ${problem.orders.length} orders)`;
    problemSelect.appendChild(option);
  });
  problemSelect.value = '3';
};

createProblemSelector();

const buildProblem = (problemId: number): ProblemData => {
  const problem = PROBLEMS.find((p) => p.id === problemId);
  if (problem) return problem;
  return PROBLEMS[PROBLEMS.length - 1];
};

const computeLossArray = (capacities: number[]): number[] => {
  const maxCapacity = Math.max(...capacities);
  const lossArray: number[] = [];
  for (let load = 0; load <= maxCapacity; ++load) {
    const value = capacities.find((capacity) => capacity >= load) ?? maxCapacity;
    lossArray[load] = value - load;
  }
  return lossArray;
};

const collectValidSlabs = (
  capacities: number[],
  widths: number[],
  colors: number[],
  lossArray: number[],
): ValidSlab[] => {
  const maxCapacity = Math.max(...capacities);
  type PartialAssignment = {
    orders: number[];
    load: number;
    colors: Set<number>;
  };

  const allAssignments: PartialAssignment[] = [{ orders: [], load: 0, colors: new Set() }];
  for (let orderId = 0; orderId < widths.length; ++orderId) {
    const newAssignments: PartialAssignment[] = [];
    const orderWidth = widths[orderId];
    const orderColor = colors[orderId];
    for (const assignment of allAssignments) {
      if (assignment.load + orderWidth > maxCapacity) {
        continue;
      }
      const assignmentColors = new Set(assignment.colors);
      assignmentColors.add(orderColor);
      if (assignmentColors.size > 2) {
        continue;
      }
      newAssignments.push({
        orders: [...assignment.orders, orderId],
        load: assignment.load + orderWidth,
        colors: assignmentColors,
      });
    }
    allAssignments.push(...newAssignments);
  }

  return allAssignments.map((assignment) => {
    const vector = new Array(widths.length).fill(0);
    assignment.orders.forEach((order) => (vector[order] = 1));
    return {
      assignment: vector,
      loss: lossArray[assignment.load],
      load: assignment.load,
    };
  });
};

const buildSatModel = (
  problem: ProblemData,
  lossArray: number[],
  breakSymmetries: boolean,
): ModelBundle => {
  const { numSlabs, numColors, orders } = problem;
  const widths = orders.map((order) => order.width);
  const colors = orders.map((order) => order.color);
  const totalLoad = widths.reduce((sum, value) => sum + value, 0);
  const maxCapacity = Math.max(...problem.capacities);
  const maxLoss = Math.max(...lossArray);

  const variables: CpModelInput['variables'] = [];
  const constraints: CpModelInput['constraints'] = [];
  const addVar = (name: string, domain: [number, number]) => {
    const idx = variables.length;
    variables.push({ name, domain });
    return idx;
  };

  const assignByOrder: number[][] = [];
  for (let orderId = 0; orderId < orders.length; ++orderId) {
    const slabVars: number[] = [];
    for (let slabId = 0; slabId < numSlabs; ++slabId) {
      slabVars.push(addVar(`assign_${orderId}_slab_${slabId}`, [0, 1]));
    }
    assignByOrder.push(slabVars);
  }

  const loadIndices = Array.from({ length: numSlabs }, (_, slabId) =>
    addVar(`load_${slabId}`, [0, maxCapacity]),
  );
  const lossIndices = Array.from({ length: numSlabs }, (_, slabId) =>
    addVar(`loss_${slabId}`, [0, maxLoss]),
  );
  const colorIndices = Array.from({ length: numSlabs }, () =>
    Array.from({ length: numColors }, (_, colorId) => addVar(`color_${colorId + 1}`, [0, 1])),
  );

  const loadLossTuples: number[][] = [];
  for (let load = 0; load <= maxCapacity; ++load) {
    loadLossTuples.push([load, lossArray[load]]);
  }
  const loadLossValues = loadLossTuples.flat();

  for (let slabId = 0; slabId < numSlabs; ++slabId) {
    const assignVars = assignByOrder.map((orderVars) => orderVars[slabId]);
    constraints.push({
      name: `load_balance_slab_${slabId}`,
      linear: {
        vars: [...assignVars, loadIndices[slabId]],
        coeffs: [
          ...widths,
          -1,
        ],
        domain: [0, 0],
      },
    });

    constraints.push({
      name: `loss_mapping_slab_${slabId}`,
      table: {
        vars: [loadIndices[slabId], lossIndices[slabId]],
        values: loadLossValues,
      },
    });
  }

  for (let orderId = 0; orderId < orders.length; ++orderId) {
    constraints.push({
      name: `assign_once_order_${orderId}`,
      linear: {
        vars: assignByOrder[orderId],
        coeffs: Array(numSlabs).fill(1),
        domain: [1, 1],
      },
    });
  }

  constraints.push({
    name: 'total_load',
    linear: {
      vars: loadIndices,
      coeffs: Array(loadIndices.length).fill(1),
      domain: [totalLoad, totalLoad],
    },
  });

  const ordersPerColor = Array.from({ length: numColors }, () => [] as number[]);
  colors.forEach((color, orderId) => {
    if (color > 0 && color <= numColors) {
      ordersPerColor[color - 1].push(orderId);
    }
  });

  for (let slabId = 0; slabId < numSlabs; ++slabId) {
    for (let colorId = 0; colorId < numColors; ++colorId) {
      const assignVars = ordersPerColor[colorId].map(
        (orderId) => assignByOrder[orderId][slabId],
      );
      if (!assignVars.length) {
        continue;
      }
      constraints.push({
        name: `color_link_slab_${slabId}_color_${colorId + 1}`,
        linear: {
          vars: [colorIndices[slabId][colorId], ...assignVars],
          coeffs: [1, ...assignVars.map(() => -1)],
          domain: [0, 1],
        },
      });
    }
  }

  for (let slabId = 0; slabId < numSlabs; ++slabId) {
    constraints.push({
      name: `max_two_colors_slab_${slabId}`,
      linear: {
        vars: colorIndices[slabId],
        coeffs: Array(colorIndices[slabId].length).fill(1),
        domain: [0, 2],
      },
    });
  }

  const uniqueColorOrders = ordersPerColor
    .map((orders) => (orders.length === 1 ? orders[0] : -1))
    .filter((orderId) => orderId >= 0);
  if (uniqueColorOrders.length) {
    for (let slabId = 0; slabId < numSlabs; ++slabId) {
      constraints.push({
        name: `unique_color_project_slab_${slabId}`,
        linear: {
          vars: uniqueColorOrders.map((orderId) => assignByOrder[orderId][slabId]),
          coeffs: Array(uniqueColorOrders.length).fill(1),
          domain: [0, 2],
        },
      });
    }
  }

  for (let slabId = 0; slabId < numSlabs - 1; ++slabId) {
    constraints.push({
      name: `load_order_slab_${slabId}`,
      linear: {
        vars: [loadIndices[slabId], loadIndices[slabId + 1]],
        coeffs: [1, -1],
        domain: [0, maxCapacity],
      },
    });
  }

  const widthToUniqueColorOrder = new Map<number, number[]>();
  const orderedEquivalentOrders: [number, number][] = [];
  for (const colorOrders of ordersPerColor) {
    if (!colorOrders.length) {
      continue;
    }
    if (colorOrders.length === 1) {
      const order = colorOrders[0];
      const width = widths[order];
      const entries = widthToUniqueColorOrder.get(width) ?? [];
      entries.push(order);
      widthToUniqueColorOrder.set(width, entries);
      continue;
    }
    const localMap = new Map<number, number[]>();
    for (const order of colorOrders) {
      const width = widths[order];
      const ordersForWidth = localMap.get(width) ?? [];
      ordersForWidth.push(order);
      localMap.set(width, ordersForWidth);
    }
    for (const ordersGroup of localMap.values()) {
      if (ordersGroup.length <= 1) continue;
      for (let idx = 0; idx < ordersGroup.length - 1; ++idx) {
        orderedEquivalentOrders.push([ordersGroup[idx], ordersGroup[idx + 1]]);
      }
    }
  }
  for (const ordersGroup of widthToUniqueColorOrder.values()) {
    if (ordersGroup.length <= 1) continue;
    for (let idx = 0; idx < ordersGroup.length - 1; ++idx) {
      orderedEquivalentOrders.push([ordersGroup[idx], ordersGroup[idx + 1]]);
    }
  }

  const positionIndices = new Map<number, number>();
  if (breakSymmetries && orderedEquivalentOrders.length) {
    for (const [first, second] of orderedEquivalentOrders) {
      if (!positionIndices.has(first)) {
        positionIndices.set(first, addVar(`position_${first}`, [0, numSlabs - 1]));
      }
      if (!positionIndices.has(second)) {
        positionIndices.set(second, addVar(`position_${second}`, [0, numSlabs - 1]));
      }
      const firstPosition = positionIndices.get(first)!;
      const secondPosition = positionIndices.get(second)!;

      const firstAssignVars = assignByOrder[first];
      const firstCoeffs = firstAssignVars.map((_, slabId) => slabId);
      constraints.push({
        name: `position_map_${first}`,
        linear: {
          vars: [...firstAssignVars, firstPosition],
          coeffs: [...firstCoeffs, -1],
          domain: [0, 0],
        },
      });

      const secondAssignVars = assignByOrder[second];
      const secondCoeffs = secondAssignVars.map((_, slabId) => slabId);
      constraints.push({
        name: `position_map_${second}`,
        linear: {
          vars: [...secondAssignVars, secondPosition],
          coeffs: [...secondCoeffs, -1],
          domain: [0, 0],
        },
      });

      constraints.push({
        name: `symmetry_${first}_${second}`,
        linear: {
          vars: [firstPosition, secondPosition],
          coeffs: [1, -1],
          domain: [-(numSlabs - 1), 0],
        },
      });
    }
  }

  const model: CpModelInput = {
    name: 'steel_mill_slab_sat',
    variables,
    constraints,
    objective: {
      vars: lossIndices,
      coeffs: Array(lossIndices.length).fill(1),
    },
  };

  const metadata: ModelMetadata = {
    solver: 'sat',
    problem,
    assignByOrder,
    loadIndices,
    lossIndices,
    colorIndices,
  };

  return { model, metadata };
};

const buildSatTableModel = (
  problem: ProblemData,
  lossArray: number[],
  breakSymmetries: boolean,
): ModelBundle => {
  const base = buildSatModel(problem, lossArray, breakSymmetries);
  const { metadata, model } = base;
  if (metadata.solver !== 'sat') {
    return base;
  }

  const validSlabs = collectValidSlabs(
    problem.capacities,
    problem.orders.map((order) => order.width),
    problem.orders.map((order) => order.color),
    lossArray,
  );
  const slabTuples = validSlabs.map((slab) => [
    ...slab.assignment,
    slab.loss,
    slab.load,
  ]);
  const tableValues = slabTuples.flat();

  for (let slabId = 0; slabId < problem.numSlabs; ++slabId) {
    const assignVars = metadata.assignByOrder!.map((vars) => vars[slabId]);
    model.constraints.push({
      name: `valid_slab_${slabId}`,
      table: {
        vars: [...assignVars, metadata.lossIndices![slabId], metadata.loadIndices![slabId]],
        values: tableValues,
      },
    });
  }

  metadata.solver = 'sat_table';
  metadata.validSlabs = validSlabs;
  return { model, metadata };
};

const buildColumnModel = (
  problem: ProblemData,
  lossArray: number[],
): ModelBundle => {
  const widths = problem.orders.map((order) => order.width);
  const totalLoad = widths.reduce((sum, value) => sum + value, 0);
  const validSlabs = collectValidSlabs(
    problem.capacities,
    widths,
    problem.orders.map((order) => order.color),
    lossArray,
  );

  const variables: CpModelInput['variables'] = [];
  const constraints: CpModelInput['constraints'] = [];
  const addVar = (name: string, domain: [number, number]) => {
    const idx = variables.length;
    variables.push({ name, domain });
    return idx;
  };

  const selectedIndices = validSlabs.map((_, idx) =>
    addVar(`column_selected_${idx}`, [0, 1]),
  );

  for (let orderId = 0; orderId < problem.orders.length; ++orderId) {
    const involvedColumns = validSlabs
      .map((slab, columnId) => (slab.assignment[orderId] === 1 ? columnId : -1))
      .filter((col) => col >= 0);
    if (!involvedColumns.length) continue;
    constraints.push({
      name: `order_cover_${orderId}`,
      linear: {
        vars: involvedColumns.map((columnId) => selectedIndices[columnId]),
        coeffs: Array(involvedColumns.length).fill(1),
        domain: [1, 1],
      },
    });
  }

  constraints.push({
    name: 'total_load',
    linear: {
      vars: selectedIndices,
      coeffs: validSlabs.map((slab) => slab.load),
      domain: [totalLoad, totalLoad],
    },
  });

  const model: CpModelInput = {
    name: 'steel_mill_slab_column',
    variables,
    constraints,
    objective: {
      vars: selectedIndices,
      coeffs: validSlabs.map((slab) => slab.loss),
    },
  };

  const metadata: ModelMetadata = {
    solver: 'sat_column',
    problem,
    selectedIndices,
    validSlabs,
  };
  return { model, metadata };
};

const buildModel = (
  solver: SolverMethod,
  problem: ProblemData,
  breakSymmetries: boolean,
): ModelBundle => {
  const lossArray = computeLossArray(problem.capacities);
  if (solver === 'sat') {
    return buildSatModel(problem, lossArray, breakSymmetries);
  }
  if (solver === 'sat_table') {
    return buildSatTableModel(problem, lossArray, breakSymmetries);
  }
  return buildColumnModel(problem, lossArray);
};

type SlabRow = {
  slabLabel: string;
  orders: string;
  load: number;
  loss: number;
  colors: string;
};

const renderTable = (rows: SlabRow[]) => {
  if (!slabTable) return;
  slabTable.innerHTML = '';
  const header = document.createElement('thead');
  header.innerHTML = `<tr>
    <th>Slab</th>
    <th>Orders</th>
    <th>Load</th>
    <th>Loss</th>
    <th>Colors</th>
  </tr>`;
  slabTable.appendChild(header);
  const body = document.createElement('tbody');
  rows.forEach((row) => {
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${row.slabLabel}</td>
      <td>${row.orders}</td>
      <td>${row.load}</td>
      <td>${row.loss}</td>
      <td>${row.colors}</td>`;
    body.appendChild(tr);
  });
  slabTable.appendChild(body);
};

const parseSolution = (response: unknown): number[] | null => {
  if (!response || typeof response !== 'object') return null;
  const candidate = (response as Record<string, unknown>).solution;
  if (!Array.isArray(candidate)) return null;
  return candidate.map((value) => {
    if (typeof value === 'number') return value;
    if (typeof value === 'string') {
      const parsed = Number.parseInt(value, 10);
      return Number.isFinite(parsed) ? parsed : 0;
    }
    return Number(value ?? 0);
  });
};

const buildAssignmentRows = (solution: number[], metadata: ModelMetadata): SlabRow[] => {
  const numSlabs = metadata.problem.numSlabs;
  const orders = metadata.problem.orders;
  const assignByOrder = metadata.assignByOrder!;
  const loadIndices = metadata.loadIndices!;
  const lossIndices = metadata.lossIndices!;
  const colorIndices = metadata.colorIndices!;

  const rows: SlabRow[] = [];
  for (let slabId = 0; slabId < numSlabs; ++slabId) {
    const assignedOrders: string[] = [];
    const colors: number[] = [];
    for (let orderId = 0; orderId < orders.length; ++orderId) {
      const varIdx = assignByOrder[orderId][slabId];
      if (solution[varIdx] === 1) {
        const order = orders[orderId];
        assignedOrders.push(`#${orderId} (w${order.width}, c${order.color})`);
      }
    }
    const colorSet = colorIndices[slabId]
      .map((varIdx, colorIdx) => (solution[varIdx] === 1 ? colorIdx + 1 : -1))
      .filter((c) => c >= 0);
    const load = solution[loadIndices[slabId]] ?? 0;
    const loss = solution[lossIndices[slabId]] ?? 0;
    rows.push({
      slabLabel: `Slab ${slabId}`,
      orders: assignedOrders.join(', ') || '—',
      load,
      loss,
      colors: colorSet.join(', ') || '—',
    });
  }
  return rows;
};

const buildColumnRows = (solution: number[], metadata: ModelMetadata): SlabRow[] => {
  const columns = metadata.validSlabs ?? [];
  const selectedIndices = metadata.selectedIndices ?? [];
  const orders = metadata.problem.orders;
  const selectedColumns = selectedIndices
    .map((varIdx, columnId) => (solution[varIdx] === 1 ? columnId : -1))
    .filter((columnId) => columnId >= 0);

  return selectedColumns.map((columnId, idx) => {
    const column = columns[columnId];
    const assignedOrders = column.assignment
      .map((value, orderId) =>
        value === 1 ? `#${orderId} (w${orders[orderId].width}, c${orders[orderId].color})` : null,
      )
      .filter((entry): entry is string => Boolean(entry));
    const colorSet = column.assignment
      .map((value, orderId) => (value === 1 ? orders[orderId].color : -1))
      .filter((color) => color >= 0);
    return {
      slabLabel: `Column ${idx}`,
      orders: assignedOrders.join(', ') || '—',
      load: column.load,
      loss: column.loss,
      colors: Array.from(new Set(colorSet)).join(', ') || '—',
    };
  });
};

const runExperiment = async () => {
  if (!runButton) return;
  runButton.disabled = true;
  if (stopButton) {
    stopButton.disabled = false;
  }
  const solverMethod = (solverSelect?.value as SolverMethod) ?? 'sat';
  const problemId = Number.parseInt(problemSelect?.value ?? '3', 10);
  const breakSymmetries = breakSymCheckbox?.checked ?? true;
  const workerCount = Math.min(
    Math.max(1, Number.parseInt(workersInput?.value ?? '1', 10)),
    maxWorkerCount,
  );
  const params = parseParamsInput(paramsInput?.value ?? '') ?? {};
  params.numSearchWorkers = workerCount;
  delete params.numWorkers;

  resetStatus();
  if (summaryEl) {
    summaryEl.textContent = 'Running…';
  }
  appendStatus(`Building ${solverMethod} model for problem ${problemId}…`);
  const problem = buildProblem(problemId);
  const bundle = buildModel(solverMethod, problem, breakSymmetries);

  try {
    let modelInstance: CpSatModelInstance;
    try {
      modelInstance = await CpSat.createModel(bundle.model as Record<string, unknown>);
    } catch (error) {
      appendStatus(`Model build failed: ${(error as Error).message}`);
      return;
    }

    const validation = await CpSat.validate(modelInstance);
    if (!validation.ok) {
      appendStatus(`Model invalid: ${validation.message}`);
      return;
    }

    appendStatus('Solving…');
    try {
      const result = await CpSat.solve(modelInstance, params);
      const response = result.response as Record<string, unknown>;
      appendStatus(`Solver response: ${JSON.stringify(response, null, 2)}`);

      const solution = parseSolution(response);
      if (!solution) {
        appendStatus('No solution data returned.');
        return;
      }

      let rows: SlabRow[] = [];
      if (bundle.metadata.solver === 'sat_column') {
        rows = buildColumnRows(solution, bundle.metadata);
      } else {
        rows = buildAssignmentRows(solution, bundle.metadata);
      }

      renderTable(rows);
      if (summaryEl) {
        summaryEl.textContent = `Solver ${bundle.metadata.solver} used ${rows.length} slabs.`;
      }
    } catch (error) {
      const err = error as Error;
      const message = err?.message ?? String(err);
      appendStatus(`Solve failed: ${message}`);
      if (err?.stack) {
        appendStatus(err.stack);
      }
      // Surface detailed error information (message + stack) in DevTools.
      console.error('[steel_mill_slab] solve failed:', message);
      if (err?.stack) {
        console.error(err.stack);
      }
    }
  } finally {
    runButton.disabled = false;
    if (stopButton) {
      stopButton.disabled = true;
    }
  }
};

if (runButton) {
  runButton.addEventListener('click', () => void runExperiment());
}

if (stopButton) {
  stopButton.addEventListener('click', () => {
    appendStatus('Cancellation requested.');
    void CpSat.cancelSolve().catch((error) => {
      appendStatus(`Cancellation failed: ${(error as Error).message}`);
    });
  });
}

if (solverSelect && breakSymCheckbox) {
  const updateBreakSymVisibility = () => {
    const isColumn = solverSelect.value === 'sat_column';
    breakSymCheckbox.disabled = isColumn;
  };
  solverSelect.addEventListener('change', updateBreakSymVisibility);
  updateBreakSymVisibility();
}

void CpSat.loadModule()
  .then(() => setReadyIndicator('Module ready.'))
  .catch((error) => {
    setReadyIndicator('Module failed to load.');
    appendStatus(`Module load error: ${(error as Error).message}`);
  });
