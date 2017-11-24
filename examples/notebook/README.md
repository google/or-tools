# Introduction
First and foremost, the Jupyter Notebook is an interactive environment for writing and running code.

# Prerequisite: Python3
These notebooks are associated with the IPython3 kernel, therefore runs Python3 code.
Thus, to run these notebooks you'll need Python3 and python ortool package.

Once you have python3 installed, ensure you have the latest pip:
```sh
pip3 install --user --upgrade pip
```
note: on Ubuntu you can also use: `apt-get install python3-pip`

# Installing Jupyter
Install Jupyter Notebook using:
```sh
pip3 install --user jupyter
```

Then install our notebooks dependencies:
```sh
pip3 install --user plotly numpy svgwrite
```

# Running the Notebooks
To run the notebook, run the following command at the Terminal (Mac/Linux) or Command Prompt (Windows):
```sh
jupyter notebook
```
You should see the notebook dashboard open in your browser.

Then go to your or-tools source directory and try to open a notebook (located in
examples/notebook).

note: You can run a code cell using Shift-Enter or pressing the `>|` button in the toolbar.
