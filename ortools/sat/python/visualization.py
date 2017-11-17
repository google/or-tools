# Copyright 2010-2017 Google
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
"""Collection of helpers to visualize cp_model solutions in colab."""

import random
from IPython.display import display
from IPython.display import SVG
import plotly.figure_factory as ff
import plotly.offline as pyo
import svgwrite


def ToDate(v):
  return '2016-01-01 6:%02i:%02i' % (v / 60, v % 60)


class ColorManager(object):
  """Utility to create colors to use in visualization."""

  def ScaledColor(self, sr, sg, sb, er, eg, eb, num_steps, step):
    """Creates an interpolated rgb color between two rgb colors."""
    num_intervals = num_steps - 1
    dr = (er - sr) / num_intervals
    dg = (eg - sg) / num_intervals
    db = (eb - sb) / num_intervals
    r = sr + dr * step
    g = sg + dg * step
    b = sb + db * step
    return 'rgb(%i, %i, %i)' % (r, g, b)

  def SeedRandomColor(self, seed=0):
    random.seed(seed)

  def RandomColor(self):
    return 'rgb(%i,%i,%i)' % (random.randint(0, 255), random.randint(0, 255),
                              random.randint(0, 255))


def DisplayJobshop(starts, durations, machines, name):
  """Simple function to display a jobshop solution using plotly."""

  jobs_count = len(starts)
  machines_count = len(starts[0])
  all_machines = range(0, machines_count)
  all_jobs = range(0, jobs_count)
  df = []
  for i in all_jobs:
    for j in all_machines:
      df.append(
          dict(
              Task='Resource%i' % machines[i][j],
              Start=ToDate(starts[i][j]),
              Finish=ToDate(starts[i][j] + durations[i][j]),
              Resource='Job%i' % i))

  sorted_df = sorted(df, key=lambda k: k['Task'])

  pyo.init_notebook_mode()

  colors = {}
  cm = ColorManager()
  cm.SeedRandomColor(0)
  for i in all_jobs:
    colors['Job%i' % i] = cm.RandomColor()

  fig = ff.create_gantt(
      sorted_df,
      colors=colors,
      index_col='Resource',
      title=name,
      show_colorbar=False,
      showgrid_x=True,
      showgrid_y=True,
      group_tasks=True)
  pyo.iplot(fig)


class SvgWrapper(object):
  """Simple SVG wrapper to use in colab."""

  def __init__(self, sizex, sizey, scaling=20.0):
    self.__sizex = sizex
    self.__sizey = sizey
    self.__scaling = scaling
    self.__offset = scaling
    self.__dwg = svgwrite.Drawing(
        size=(self.__sizex * self.__scaling + self.__offset,
              self.__sizey * self.__scaling + self.__offset * 2))

  def Display(self):
    display(SVG(self.__dwg.tostring()))

  def AddRectangle(self, x, y, dx, dy, fill, stroke='black', label=None):
    """Draw a rectangle, dx and dy must be >= 0."""
    s = self.__scaling
    o = self.__offset
    corner = (x * s + o, (self.__sizey - y - dy) * s + o)
    size = (dx * s - 1, dy * s - 1)
    self.__dwg.add(
        self.__dwg.rect(insert=corner, size=size, fill=fill, stroke=stroke))
    self.AddText(x + 0.5 * dx, y + 0.5 * dy, label)

  def AddText(self, x, y, label):
    text = self.__dwg.text(
        label,
        insert=(x * self.__scaling + self.__offset,
                (self.__sizey - y) * self.__scaling + self.__offset),
        text_anchor='middle',
        font_family='sans-serif',
        font_size='%dpx' % (self.__scaling / 2))
    self.__dwg.add(text)

  def AddXScale(self, step=1):
    """Add an scale on the x axis."""
    o = self.__offset
    s = self.__scaling
    y = self.__sizey * s + o / 2.0 + o
    dy = self.__offset / 4.0
    self.__dwg.add(
        self.__dwg.line((o, y), (self.__sizex * s + o, y), stroke='black'))
    for i in range(0, int(self.__sizex) + 1, step):
      self.__dwg.add(
          self.__dwg.line(
              (o + i * s, y - dy), (o + i * s, y + dy), stroke='black'))

  def AddYScale(self, step=1):
    """Add an scale on the y axis."""
    o = self.__offset
    s = self.__scaling
    x = o / 2.0
    dx = self.__offset / 4.0
    self.__dwg.add(
        self.__dwg.line((x, o), (x, self.__sizey * s + o), stroke='black'))
    for i in range(0, int(self.__sizey) + 1, step):
      self.__dwg.add(
          self.__dwg.line((x - dx, i * s + o), (x + dx, i * s + o),
                          stroke='black'))

  def AddTitle(self, title):
    """Add a title to the drawing."""
    text = self.__dwg.text(
        title,
        insert=(self.__offset + self.__sizex * self.__scaling / 2.0,
                self.__offset / 2),
        text_anchor='middle',
        font_family='sans-serif',
        font_size='%dpx' % (self.__scaling / 2))
    self.__dwg.add(text)

