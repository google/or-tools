import { CpSat, type CpSatModelInstance } from '../lib';

type Domain = [number, number] | number[];
type LinearConstraint = {
  name?: string;
  linear: { vars: number[]; coeffs: number[]; domain: number[] };
};
type TableConstraint = {
  name?: string;
  table: { vars: number[]; values: number[]; negate?: boolean };
};
type CpModelInput = {
  name: string;
  variables: Array<{ name: string; domain: Domain }>;
  constraints: Array<LinearConstraint | TableConstraint>;
  objective?: { vars: number[]; coeffs: number[]; offset?: number; scaling_factor?: number };
};

type ScheduleCell = { opponent: number; home: boolean };
type ScheduleGrid = ScheduleCell[][];

type SchedulingBuild = {
  model: CpModelInput;
  fixtures: number[][][];
  numTeams: number;
  numDays: number;
};

const statusEl = document.getElementById('status') as HTMLPreElement | null;
const scheduleMessage = document.getElementById('schedule-message') as HTMLElement | null;
const scheduleTable = document.getElementById('schedule-table') as HTMLTableElement | null;
const teamsInput = document.getElementById('teams') as HTMLInputElement | null;
const workerInput = document.getElementById('workers') as HTMLInputElement | null;
const workerBridgeToggle = document.getElementById('use-worker-bridge') as HTMLInputElement | null;
const runButton = document.getElementById('run') as HTMLButtonElement | null;
const stopButton = document.getElementById('stop') as HTMLButtonElement | null;
const readyIndicator = document.getElementById('ready-indicator') as HTMLElement | null;
const maxWorkerCount = Math.max(1, navigator.hardwareConcurrency || 1);
if (workerInput) {
  workerInput.max = String(maxWorkerCount);
  workerInput.min = '1';
  workerInput.value = String(maxWorkerCount);
}
if (workerBridgeToggle) {
  applyWorkerBridgePreference(true);
  workerBridgeToggle.addEventListener('change', () => {
    const enabled = workerBridgeToggle.checked;
    applyWorkerBridgePreference(enabled);
  });
}

  function applyWorkerBridgePreference(enabled: boolean) {
    if (workerBridgeToggle) {
      workerBridgeToggle.checked = enabled;
    }
    if (typeof CpSat?.setWorkerBridgeEnabled === 'function') {
      CpSat.setWorkerBridgeEnabled(enabled);
    }
  }

  function append(text: string) {
    if (statusEl) {
      statusEl.textContent += `${text}\n`;
    }
  }

  function setRunning(running: boolean) {
    if (runButton) {
      runButton.disabled = running;
    }
    if (stopButton) {
      stopButton.disabled = !running;
    }
  }

  function showScheduleMessage(message: string) {
    if (scheduleMessage) {
      scheduleMessage.textContent = message;
    }
    if (scheduleTable) {
      scheduleTable.innerHTML = '';
      scheduleTable.classList.toggle('hidden', true);
    }
  }

  function renderSchedule(schedule: ScheduleGrid, numDays: number) {
    if (!scheduleTable) return;
    scheduleTable.innerHTML = '';
    scheduleTable.classList.remove('hidden');

    const thead = document.createElement('thead');
    const headRow = document.createElement('tr');
    const teamHeader = document.createElement('th');
    teamHeader.textContent = 'Team';
    headRow.appendChild(teamHeader);
    for (let day = 0; day < numDays; ++day) {
      const cell = document.createElement('th');
      cell.textContent = `Day ${day + 1}`;
      headRow.appendChild(cell);
    }
    thead.appendChild(headRow);
    scheduleTable.appendChild(thead);

    const tbody = document.createElement('tbody');
    schedule.forEach((row, team) => {
      const tr = document.createElement('tr');
      const teamCell = document.createElement('th');
      teamCell.textContent = `Team ${team}`;
      tr.appendChild(teamCell);
      row.forEach((match) => {
        const td = document.createElement('td');
        if (match.opponent >= 0) {
          td.textContent = match.home ? `vs ${match.opponent}` : `@ ${match.opponent}`;
          td.dataset.location = match.home ? 'home' : 'away';
        } else {
          td.textContent = '—';
        }
        tr.appendChild(td);
      });
      tbody.appendChild(tr);
    });
    scheduleTable.appendChild(tbody);

    if (scheduleMessage) {
      scheduleMessage.textContent = 'Teams play opponents once per half, minimizing total breaks.';
    }
  }

  function parseSolution(solution: unknown): number[] | null {
    if (!Array.isArray(solution)) return null;
    return solution.map((value) => {
      if (typeof value === 'number') return value;
      const parsed = Number.parseInt(String(value), 10);
      return Number.isFinite(parsed) ? parsed : 0;
    });
  }

  function createSchedulingModel(numTeams: number): SchedulingBuild {
    if (numTeams % 2 !== 0) {
      throw new Error('Number of teams must be even.');
    }
    if (numTeams < 2) {
      throw new Error('At least two teams are required.');
    }

    const numDays = 2 * numTeams - 2;
    const matchesPerHalf = numTeams - 1;

    const variables: CpModelInput['variables'] = [];
    const constraints: CpModelInput['constraints'] = [];

    const addVar = (name: string, domain: Domain) => {
      const index = variables.length;
      variables.push({ name, domain });
      return index;
    };

    const addLinear = (name: string, vars: number[], coeffs: number[], domain: number[]) => {
      constraints.push({ name, linear: { vars, coeffs, domain } });
    };

    const fixtures: number[][][] = [];
    for (let day = 0; day < numDays; ++day) {
      fixtures[day] = [];
      for (let home = 0; home < numTeams; ++home) {
        fixtures[day][home] = [];
        for (let away = 0; away < numTeams; ++away) {
          if (home === away) {
            fixtures[day][home][away] = -1;
          } else {
            fixtures[day][home][away] = addVar(
              `fixture_d${day}_h${home}_a${away}`,
              [0, 1],
            );
          }
        }
      }
    }

    const atHome: number[][] = [];
    for (let day = 0; day < numDays; ++day) {
      atHome[day] = [];
      for (let team = 0; team < numTeams; ++team) {
        atHome[day][team] = addVar(`at_home_d${day}_t${team}`, [0, 1]);
      }
    }

    // Each team plays exactly one opponent per day.
    for (let day = 0; day < numDays; ++day) {
      for (let team = 0; team < numTeams; ++team) {
        const vars: number[] = [];
        const coeffs: number[] = [];
        for (let other = 0; other < numTeams; ++other) {
          if (team === other) continue;
          vars.push(fixtures[day][team][other]);
          coeffs.push(1);
          vars.push(fixtures[day][other][team]);
          coeffs.push(1);
        }
        addLinear(`plays_once_d${day}_t${team}`, vars, coeffs, [1, 1]);
      }
    }

    // Each ordered pair plays exactly once per season.
    for (let team = 0; team < numTeams; ++team) {
      for (let other = 0; other < numTeams; ++other) {
        if (team === other) continue;
        const vars: number[] = [];
        const coeffs: number[] = [];
        for (let day = 0; day < numDays; ++day) {
          vars.push(fixtures[day][team][other]);
          coeffs.push(1);
        }
        addLinear(`once_per_season_t${team}_o${other}`, vars, coeffs, [1, 1]);
      }
    }

    // Split season: encounter each opponent once per half.
    for (let team = 0; team < numTeams; ++team) {
      for (let other = 0; other < numTeams; ++other) {
        if (team === other) continue;
        const firstHalfVars: number[] = [];
        const firstHalfCoeffs: number[] = [];
        const secondHalfVars: number[] = [];
        const secondHalfCoeffs: number[] = [];
        for (let day = 0; day < matchesPerHalf; ++day) {
          firstHalfVars.push(fixtures[day][team][other]);
          firstHalfCoeffs.push(1);
          firstHalfVars.push(fixtures[day][other][team]);
          firstHalfCoeffs.push(1);

          const secondDay = day + matchesPerHalf;
          secondHalfVars.push(fixtures[secondDay][team][other]);
          secondHalfCoeffs.push(1);
          secondHalfVars.push(fixtures[secondDay][other][team]);
          secondHalfCoeffs.push(1);
        }
        addLinear(`first_half_${team}_${other}`, firstHalfVars, firstHalfCoeffs, [1, 1]);
        addLinear(`second_half_${team}_${other}`, secondHalfVars, secondHalfCoeffs, [1, 1]);
      }
    }

    // Link at_home variables.
    for (let day = 0; day < numDays; ++day) {
      for (let team = 0; team < numTeams; ++team) {
        const vars: number[] = [];
        const coeffs: number[] = [];
        for (let other = 0; other < numTeams; ++other) {
          if (team === other) continue;
          vars.push(fixtures[day][team][other]);
          coeffs.push(1);
        }
        vars.push(atHome[day][team]);
        coeffs.push(-1);
        addLinear(`link_home_d${day}_t${team}`, vars, coeffs, [0, 0]);
      }
    }

    // No three consecutive homes or aways.
    for (let team = 0; team < numTeams; ++team) {
      for (let start = 0; start < numDays - 2; ++start) {
        const homeVars: number[] = [];
        const coeffs = [1, 1, 1];
        homeVars.push(atHome[start][team], atHome[start + 1][team], atHome[start + 2][team]);
        addLinear(`no_three_home_t${team}_s${start}`, homeVars, coeffs, [0, 2]);
        addLinear(`no_three_away_t${team}_s${start}`, homeVars, coeffs, [1, 3]);
      }
    }

    const breakVars: number[] = [];
    for (let team = 0; team < numTeams; ++team) {
      for (let day = 0; day < numDays - 1; ++day) {
        const breakVar = addVar(`break_t${team}_d${day}`, [0, 1]);
        breakVars.push(breakVar);
        constraints.push({
          name: `break_table_t${team}_d${day}`,
          table: {
            vars: [atHome[day][team], atHome[day + 1][team], breakVar],
            values: [0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1],
          },
        });
      }
    }

    if (breakVars.length) {
      const coeffs = Array(breakVars.length).fill(1);
      const minBreaks = Math.max(0, 2 * numTeams - 4);
      addLinear('min_breaks', breakVars, coeffs, [minBreaks, breakVars.length]);
    }

    const objective =
        breakVars.length > 0
          ? {
              vars: breakVars,
              coeffs: Array(breakVars.length).fill(1),
              offset: 0,
              scaling_factor: 1,
            }
          : undefined;

    return {
      model: {
        name: `sports_scheduling_${numTeams}`,
        variables,
        constraints,
        objective,
      },
      fixtures,
      numTeams,
      numDays,
    };
  }

  function extractSchedule(
    solution: number[],
    fixtures: number[][][],
    numTeams: number,
    numDays: number,
  ): ScheduleGrid {
    const schedule: ScheduleGrid = [];
    const readValue = (index: number) => solution[index] ?? 0;

    for (let team = 0; team < numTeams; ++team) {
      const row: ScheduleCell[] = [];
      for (let day = 0; day < numDays; ++day) {
        let opponent = -1;
        let home = false;
        for (let other = 0; other < numTeams; ++other) {
          if (team === other) continue;
          const homeVar = fixtures[day][team][other];
          if (homeVar >= 0 && readValue(homeVar) === 1) {
            opponent = other;
            home = true;
            break;
          }
          const awayVar = fixtures[day][other][team];
          if (awayVar >= 0 && readValue(awayVar) === 1) {
            opponent = other;
            home = false;
            break;
          }
        }
        row.push({ opponent, home });
      }
      schedule.push(row);
    }
    return schedule;
  }

  async function runSportsScheduling() {
    if (!teamsInput || !workerInput) {
      append('Missing configuration inputs.');
      return;
    }

    const numTeams = Math.max(2, Number.parseInt(teamsInput.value, 10) || 2);
    if (numTeams % 2 !== 0) {
      append('Number of teams must be even.');
      showScheduleMessage('Please enter an even number of teams (≥2).');
      return;
    }

    const requestedWorkers = Number.parseInt(workerInput.value, 10) || 1;
    const workers = Math.min(Math.max(1, requestedWorkers), maxWorkerCount);
    workerInput.value = String(workers);
    append(`Building sports scheduling model (teams=${numTeams})…`);
    showScheduleMessage('Solving…');

    setRunning(true);
    try {
      let build: SchedulingBuild;
      try {
        build = createSchedulingModel(numTeams);
      } catch (err) {
        append(`Model creation failed: ${(err as Error).message}`);
        showScheduleMessage('Model creation failed.');
        return;
      }

      let model: CpSatModelInstance;
      try {
        model = await CpSat.createModel(build.model);
      } catch (err) {
        append(`Model build failed: ${(err as Error).message}`);
        showScheduleMessage('Model build failed.');
        return;
      }

      const validation = await CpSat.validate(model);
      if (!validation.ok) {
        append(`Model invalid: ${validation.message}`);
        showScheduleMessage('Model invalid.');
        return;
      }

      const params: Record<string, unknown> = {};
      if (workers > 0) {
        params.num_search_workers = workers;
      }
      console.debug('[SportsScheduling] solve params', {
        num_search_workers: params.num_search_workers ?? 'default',
      });

      append('Solving…');
      try {
        const result = await CpSat.solve(model, params);
        if (!result.response || !statusEl) {
          append('Solver returned no response.');
          showScheduleMessage('Solver returned no response.');
          return;
        }
        statusEl.textContent = JSON.stringify(result.response, null, 2);
        const values = parseSolution((result.response as Record<string, unknown>).solution);
        if (!values) {
          showScheduleMessage('Unable to parse solver solution.');
          return;
        }
        const schedule = extractSchedule(values, build.fixtures, build.numTeams, build.numDays);
        renderSchedule(schedule, build.numDays);
      } catch (err) {
        append(`Solve failed: ${(err as Error).message}`);
        showScheduleMessage('Solve failed.');
      }
    } finally {
      setRunning(false);
    }
  }

  if (runButton) {
    runButton.addEventListener('click', () => {
      void runSportsScheduling();
    });
  }

  if (stopButton) {
    stopButton.disabled = true;
    stopButton.addEventListener('click', () => {
      append('Cancellation requested.');
      void CpSat.cancelSolve().catch((err) => {
        append(`Cancellation failed: ${(err as Error).message}`);
      });
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
    showScheduleMessage('Module failed to load.');
  });
