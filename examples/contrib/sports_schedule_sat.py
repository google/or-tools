# Based on sports_scheduling_sat.cc, Copyright 2010-2021 Google LLC
#
# Translated to Python by James E. Marca August 2019
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Sports scheduling problem.

We want to solve the problem of scheduling of team matches in a
double round robin tournament.  Given a number of teams, we want
each team to encounter all other teams, twice, once at home, and
once away. Furthermore, you cannot meet the same team twice in the
same half-season.

Finally, there are constraints on the sequence of home or aways:
 - You cannot have 3 consecutive homes or three consecutive aways.
 - A break is a sequence of two homes or two aways, the overall objective
   of the optimization problem is to minimize the total number of breaks.
 - If team A meets team B, the reverse match cannot happen less that 6 weeks
   after.

Translation to python:

This version is essentially a straight translation of
sports_scheduling_sat.cc from the C++ example, SecondModel version.

This code gets an F from code climate for code quality and
maintainability.

Originally developed with pool vs pool constraints on the mailing
list, but that has been dropped for this version to keep it in sync
with the C++ version.

Added command line options to set numbers of teams, numbers of days,
etc.

Added CSV output.

For a version with pool constraints, plus tests, etc, see
https://github.com/jmarca/sports_scheduling

"""
import argparse
import os
import re
import csv
import math

from ortools.sat.python import cp_model


def csv_dump_results(solver, fixtures, num_teams, num_matchdays, csv_basename):
    matchdays = range(num_matchdays)
    teams = range(num_teams)

    vcsv = []
    for d in matchdays:
        game = 0
        for home in range(num_teams):
            for away in range(num_teams):
                if solver.Value(fixtures[d][home][away]):
                    game += 1
                    # each row: day,game,home,away
                    row = {
                        'day': d + 1,
                        'game': game,
                        'home': home + 1,
                        'away': away + 1
                    }
                    vcsv.append(row)

    # check for any existing file
    idx = 1
    checkname = csv_basename
    match = re.search(r"\.csv", checkname)
    if not match:
        print(
            'looking for a .csv ending in passed in CSV file name.  Did not find it, so appending .csv to',
            csv_basename)
        csv_basename += ".csv"

    checkname = csv_basename
    while os.path.exists(checkname):
        checkname = re.sub(r"\.csv", "_{}.csv".format(idx), csv_basename)
        idx += 1
        # or just get rid of it, but that is often undesireable
        # os.unlink(csv_basename)

    with open(checkname, 'w', newline='') as csvfile:
        fieldnames = ['day', 'game', 'home', 'away']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

        writer.writeheader()
        for row in vcsv:
            writer.writerow(row)


def screen_dump_results(solver, fixtures, num_teams, num_matchdays):
    matchdays = range(num_matchdays)
    teams = range(num_teams)

    total_games = 0
    for d in matchdays:
        game = 0
        for home in teams:
            for away in teams:
                match_on = solver.Value(fixtures[d][home][away])
                if match_on:
                    game += 1
                    print('day %i game %i home %i away %i' %
                          (d + 1, game, home + 1, away + 1))
        total_games += game


def assign_matches(num_teams,
                   num_matchdays,
                   num_matches_per_day,
                   max_home_stand,
                   time_limit=None,
                   num_cpus=None,
                   csv=None,
                   debug=None):
    """Assign matches between teams in a league.

    Keyword arguments:
    num_teams -- the number of teams
    num_matchdays -- the number of match days to play.  Should be greater than one day.  Note that if num_matchdays is exactly some multipe (`n`) of `num_teams - 1` then each team with play every other team exactly `n` times.  If the number of match days is less than or greater than a perfect multiple, then some teams will not play each other `n` times.
    num_matches_per_day -- how many matches can be played in a day.  The assumption is one match per day, and really this code was not tested with different values.
    max_home_stand -- how many home games are allowed to be in a row.
    time_limit -- the time in minutes to allow the solver to work on the problem.
    num_cpus -- the number of processors to use for the solution
    csv -- a file name to save the output to a CSV file
    debug -- boolean value stating whether to ask the solver to show its progress or not

    """

    model = cp_model.CpModel()

    print('num_teams', num_teams, 'num_matchdays', num_matchdays,
          'num_matches_per_day', num_matches_per_day, 'max_home_stand',
          max_home_stand)

    matchdays = range(num_matchdays)
    matches = range(num_matches_per_day)
    teams = range(num_teams)
    # how many possible unique games?
    unique_games = (num_teams) * (num_teams - 1) / 2

    # how many games are possible to play
    total_games = num_matchdays * num_matches_per_day

    # maximum possible games versus an opponent.  example, if 20
    # possible total games, and 28 unique combinations, then 20 // 28
    # +1 = 1.  If 90 total games (say 5 per day for 18 match days) and
    # 10 teams for 45 possible unique combinations of teams, then 90
    # // 45 + 1 = 3. Hmm.  Should be 2
    matchups = int((total_games // unique_games) + 1)
    # print(matchups)
    # there is a special case, if total games / unique games == total
    # games // unique games, then the constraint can be ==, not <=
    matchups_exact = False
    if (total_games % unique_games == 0):
        matchups_exact = True
        matchups = int(total_games // unique_games)

    print('expected matchups per pair', matchups, 'exact?', matchups_exact)

    days_to_play = int(unique_games // num_matches_per_day)
    print('unique_games', unique_games, '\nnum matches per day',
          num_matches_per_day, '\ndays to play', days_to_play,
          '\ntotal games possible', total_games)

    fixtures = [
    ]  # all possible games, list of lists of lists: fixture[day][iteam][jteam]
    at_home = [
    ]  # whether or not a team plays at home on matchday, list of lists

    # Does team i receive team j at home on day d?
    for d in matchdays:
        # hackity hack, append a new list for all possible fixtures for a team on day d
        fixtures.append([])
        for i in teams:
            # hackity hack, append a list of possible fixtures for team i
            fixtures[d].append([])
            for j in teams:
                # for each possible away opponent for home team i, add a fixture
                #
                # note that the fixture is not only that team i plays
                # team j, but also that team i is the home team and
                # team j is the away team.
                fixtures[d][i].append(
                    model.NewBoolVar(
                        'fixture: home team %i, opponent %i, matchday %i' %
                        (i, j, d)))
                if i == j:
                    # It is not possible for team i to play itself,
                    # but it is cleaner to add the fixture than it is
                    # to skip it---the list stays the length of the
                    # number of teams.  The C++ version adds a "FalseVar" instead
                    model.Add(fixtures[d][i][j] == 0)  # forbid playing self

    # Is team t at home on day d?
    for d in matchdays:
        # hackity hack, append a new list for whether or not a team is at home on day d
        at_home.append([])
        for i in teams:
            # is team i playing at home on day d?
            at_home[d].append(
                model.NewBoolVar('team %i is home on matchday %i' % (i, d)))

    # each day, team t plays either home or away, but only once
    for d in matchdays:
        for t in teams:
            # for each team, each day, list possible opponents
            possible_opponents = []
            for opponent in teams:
                if t == opponent:
                    continue
                # t is home possibility
                possible_opponents.append(fixtures[d][t][opponent])
                # t is away possibility
                possible_opponents.append(fixtures[d][opponent][t])
            model.Add(
                sum(possible_opponents) == 1)  # can only play one game per day

    # "Each fixture happens once per season" is not a valid constraint
    # in this formulation.  in the C++ program, there are exactly a
    # certain number of games such that every team plays every other
    # team once at home, and once away.  In this case, this is not the
    # case, because there are a variable number of games.  Instead,
    # the constraint is that each fixture happens Matchups/2 times per
    # season, where Matchups is the number of times each team plays every
    # other team.
    fixture_repeats = int(math.ceil(matchups / 2))
    print('fixture repeats expected is', fixture_repeats)

    for t in teams:
        for opponent in teams:
            if t == opponent:
                continue
            possible_days = []
            for d in matchdays:
                possible_days.append(fixtures[d][t][opponent])
            if matchups % 2 == 0 and matchups_exact:
                model.Add(sum(possible_days) == fixture_repeats)
            else:
                # not a hard constraint, because not exactly the right
                # number of matches to be played
                model.Add(sum(possible_days) <= fixture_repeats)

    # Next, each matchup between teams happens at most "matchups"
    # times per season. Again this is different that C++ version, in
    # which the number of games is such that each team plays every
    # other team exactly two times.  Here this is not the case,
    # because the number of games in the season is not fixed.
    #
    # in C++ version, the season is split into two halves.  In this
    # case, splitting the season into "Matchups" sections, with each
    # team playing every other team once per section.
    #
    # The very last section of games is special, as there might not be
    # enough games for every team to play every other team once.
    #
    for t in teams:
        for opponent in teams:
            if t == opponent:
                continue
            prior_home = []
            for m in range(matchups):
                current_home = []
                pairings = []
                # if m = matchups - 1, then last time through
                days = int(days_to_play)
                if m == matchups - 1:
                    days = int(
                        min(days_to_play, num_matchdays - m * days_to_play))
                # print('days',days)
                for d in range(days):
                    theday = int(d + m * days_to_play)
                    # print('theday',theday)
                    pairings.append(fixtures[theday][t][opponent])
                    pairings.append(fixtures[theday][opponent][t])
                    # current_home.append(fixtures[theday][t][opponent])
                if m == matchups - 1 and not matchups_exact:
                    # in the last group of games, if the number of
                    # games left to play isn't quite right, then it
                    # will not be possible for each team to play every
                    # other team, and so the sum will be <= 1, rather
                    # than == 1
                    #
                    # print('last matchup',m,'relaxed pairings constraint')
                    model.Add(sum(pairings) <= 1)
                else:
                    # if it is not the last group of games, then every
                    # team must play every other team exactly once.
                    #
                    # print('matchup',m,'hard pairings constraint')
                    model.Add(sum(pairings) == 1)

    # maintain consistency between fixtures and at_home[day][team]
    for d in matchdays:
        for t in teams:
            for opponent in teams:
                if t == opponent:
                    continue
                # if the [t][opp] fixture is true, then at home is true for t
                model.AddImplication(fixtures[d][t][opponent], at_home[d][t])
                # if the [t][opp] fixture is true, then at home false for opponent
                model.AddImplication(fixtures[d][t][opponent],
                                     at_home[d][opponent].Not())

    # balance home and away games via the following "breaks" logic
    # forbid sequence of "max_home_stand" home games or away games in a row
    # In sports like baseball, homestands can be quite long.
    for t in teams:
        for d in range(num_matchdays - max_home_stand):
            model.AddBoolOr([
                at_home[d + offset][t] for offset in range(max_home_stand + 1)
            ])
            model.AddBoolOr([
                at_home[d + offset][t].Not()
                for offset in range(max_home_stand + 1)
            ])
            # note, this works because AddBoolOr means at least one
            # element must be true.  if it was just AddBoolOr([home0,
            # home1, ..., homeN]), then that would mean that one or
            # all of these could be true, and you could have an
            # infinite sequence of home games.  However, that home
            # constraint is matched with an away constraint.  So the
            # combination says:
            #
            # AddBoolOr([home0, ... homeN]) at least one of these is true
            # AddBoolOr([away0, ... awayN]) at least one of these is true
            #
            # taken together, at least one home from 0 to N is true,
            # which means at least one away0 to awayN is false.  At
            # the same time, at least one away is true, which means
            # that the corresponding home is false.  So together, this
            # prevents a sequence of one more than max_home_stand to
            # take place.

    # objective using breaks concept
    breaks = []
    for t in teams:
        for d in range(num_matchdays - 1):
            breaks.append(
                model.NewBoolVar(
                    'two home or two away for team %i, starting on matchday %i'
                    % (t, d)))

            model.AddBoolOr([at_home[d][t], at_home[d + 1][t], breaks[-1]])
            model.AddBoolOr(
                [at_home[d][t].Not(), at_home[d + 1][t].Not(), breaks[-1]])

            model.AddBoolOr(
                [at_home[d][t].Not(), at_home[d + 1][t], breaks[-1].Not()])
            model.AddBoolOr(
                [at_home[d][t], at_home[d + 1][t].Not(), breaks[-1].Not()])

            # I couldn't figure this out, so I wrote a little program
            # and proved it.  These effectively are identical to
            #
            # model.Add(at_home[d][t] == at_home[d+1][t]).OnlyEnforceIf(breaks[-1])
            # model.Add(at_home[d][t] != at_home[d+1][t]).OnlyEnforceIf(breaks[-1].Not())
            #
            # except they are a little more efficient, I believe.  Wrote it up in a blog post
            #
            # my write-up is at https://activimetrics.com/blog/ortools/cp_sat/addboolor/

    # constrain breaks
    #
    # Another deviation from the C++ code.  In the C++, the breaks sum is
    # required to be >= (2 * num_teams - 4), which is exactly the number
    # of match days.
    #
    # I said on the mailing list that I didn't know why this was, and
    # Laurent pointed out that there was a known bound.  I looked it
    # up and found some references (see thread at
    # https://groups.google.com/d/msg/or-tools-discuss/ITdlPs6oRaY/FvwgB5LgAQAJ)
    #
    # The reference states that "schedules with n teams with even n
    # have at least n − 2 breaks", and further that "for odd n
    # schedules without any breaks are constructed."
    #
    # That research doesn't *quite* apply here, as the authors were
    # assuming a single round-robin tournament
    #.
    # Here there is not an exact round-robin tournament multiple, but
    # still the implication is that the number of breaks cannot be
    # less than the number of matchdays.
    #
    # Literature aside, I'm finding in practice that if you don't have
    # an exact round robin multiple, and if num_matchdays is odd, the
    # best you can do is num_matchdays + 1.  If you have even days,
    # then you can do num_matchdays.  This can be tested for small
    # numbers of teams and days, in which the solver is able to search
    # all possible combinations in a reasonable time limit.

    optimal_value = matchups * (num_teams - 2)
    if not matchups_exact:
        # fiddle a bit, based on experiments with low values of N and D
        if num_matchdays % 2:
            # odd number of days
            optimal_value = min(num_matchdays + 1, optimal_value)
        else:
            optimal_value = min(num_matchdays, optimal_value)

    print('expected optimal value is', optimal_value)
    model.Add(sum(breaks) >= optimal_value)

    model.Minimize(sum(breaks))
    # run the solver
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = time_limit
    solver.parameters.log_search_progress = debug
    solver.parameters.num_search_workers = num_cpus

    # solution_printer = SolutionPrinter() # since we stop at first
    # solution, this isn't really
    # necessary I think
    status = solver.Solve(model)
    print('Solve status: %s' % solver.StatusName(status))
    print('Statistics')
    print('  - conflicts : %i' % solver.NumConflicts())
    print('  - branches  : %i' % solver.NumBranches())
    print('  - wall time : %f s' % solver.WallTime())

    if status == cp_model.INFEASIBLE:
        return status

    if status == cp_model.UNKNOWN:
        print('Not enough time allowed to compute a solution')
        print('Add more time using the --timelimit command line option')
        return status

    print('Optimal objective value: %i' % solver.ObjectiveValue())

    screen_dump_results(solver, fixtures, num_teams, num_matchdays)

    if status != cp_model.OPTIMAL and solver.WallTime() >= time_limit:
        print('Please note that solver reached maximum time allowed %i.' %
              time_limit)
        print(
            'A better solution than %i might be found by adding more time using the --timelimit command line option'
            % solver.ObjectiveValue())

    if csv:
        csv_dump_results(solver, fixtures, num_teams, num_matchdays, csv)

    # # print break results, to get a clue what they are doing
    # print('Breaks')
    # for b in breaks:
    #     print('  %s is %i' % (b.Name(), solver.Value(b)))


def main():
    """Entry point of the program."""
    parser = argparse.ArgumentParser(
        description='Solve sports league match play assignment problem')
    parser.add_argument('-t,--teams',
                        type=int,
                        dest='num_teams',
                        default=10,
                        help='Number of teams in the league')

    parser.add_argument(
        '-d,--days',
        type=int,
        dest='num_matchdays',
        default=2 * 10 - 2,
        help=
        'Number of days on which matches are played.  Default is enough days such that every team can play every other team, or (number of teams - 1)'
    )

    parser.add_argument(
        '--matches_per_day',
        type=int,
        dest='num_matches_per_day',
        default=10 - 1,
        help=
        'Number of matches played per day.  Default is number of teams divided by 2.  If greater than the number of teams, then this implies some teams will play each other more than once.  In that case, home and away should alternate between the teams in repeated matchups.'
    )

    parser.add_argument(
        '--csv',
        type=str,
        dest='csv',
        default='output.csv',
        help='A file to dump the team assignments.  Default is output.csv')

    parser.add_argument(
        '--timelimit',
        type=int,
        dest='time_limit',
        default=60,
        help='Maximum run time for solver, in seconds.  Default is 60 seconds.')

    parser.add_argument(
        '--cpu',
        type=int,
        dest='cpu',
        help=
        'Number of workers (CPUs) to use for solver.  Default is 6 or number of CPUs available, whichever is lower'
    )

    parser.add_argument('--debug',
                        action='store_true',
                        help="Turn on some print statements.")

    parser.add_argument(
        '--max_home_stand',
        type=int,
        dest='max_home_stand',
        default=2,
        help=
        "Maximum consecutive home or away games.  Default to 2, which means three home or away games in a row is forbidden."
    )

    args = parser.parse_args()

    # set default for num_matchdays
    num_matches_per_day = args.num_matches_per_day
    if not num_matches_per_day:
        num_matches_per_day = args.num_teams // 2
    ncpu = 8
    try:
        ncpu = len(os.sched_getaffinity(0))
    except AttributeError:
        pass
    cpu = args.cpu
    if not cpu:
        cpu = min(6, ncpu)
        print('Setting number of search workers to %i' % cpu)

    if cpu > ncpu:
        print(
            'You asked for %i workers to be used, but the os only reports %i CPUs available.  This might slow down processing'
            % (cpu, ncpu))

    if cpu != 6:
        # don't whinge at user if cpu is set to 6
        if cpu < ncpu:
            print(
                'Using %i workers, but there are %i CPUs available.  You might get faster results by using the command line option --cpu %i, but be aware ORTools CP-SAT solver is tuned to 6 CPUs'
                % (cpu, ncpu, ncpu))

        if cpu > 6:
            print(
                'Using %i workers.  Be aware ORTools CP-SAT solver is tuned to 6 CPUs'
                % cpu)

    # assign_matches()
    assign_matches(args.num_teams, args.num_matchdays, num_matches_per_day,
                   args.max_home_stand, args.time_limit, cpu, args.csv,
                   args.debug)


if __name__ == '__main__':
    main()
