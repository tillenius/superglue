#!/usr/bin/env python

import sys
import re

#from matplotlib import rc
#
#rc('text', usetex=True)
#rc('font', **{'family':'serif', 'serif':['Computer Modern Roman'], 
#                                'monospace': ['Computer Modern Typewriter']})

##################################################
# CONFIG
##################################################

# which tasks to show (return true for desired tasks)
def myfilter(x):
    return True

fillcols = ['#ff0000', '#00ff00', '#0000ff', '#ffff00', '#ff00ff', '#00ffff']
edgecols = ['#000000', '#000000', '#000000', '#000000', '#000000', '#000000']
coldict = dict()
nextcol = 0

# which color to use for each task (fillcolor, edgecolor)
def getColor(text):
    global nextcol

    spl = text.split();
    if len(spl) > 1:
        text = spl[0]

    # hard-code task names to colors
    if text.startswith("read"): return ['#000000', '#000000']

    # distribute colors cyclically
    if text in coldict:
        colidx = coldict[text]
    else:
        colidx = nextcol
        nextcol = (nextcol + 1) % len(fillcols)
        coldict[text] = colidx
    return [fillcols[colidx], edgecols[colidx]]

# if any text is to be attached to the task
def getText(text):
    return []

def getExtra(text):
  m = re.match(r'.* tasks: ([0-9]+)', text)
  if m:
    return m.group(1)
  return []

##################################################
def load_file(filename):
    fh = open(filename, "r")
    out = list()
    pattern = re.compile('([0-9]+): ([0-9]+) ([0-9]+) (.*)')
    mpipattern = re.compile('([0-9]+) ([0-9]+): ([0-9]+) ([0-9]+) (.*)')
    cpattern = re.compile('(.*) \((.*)\)')
    while True:
        line = fh.readline()
        if not line:
          break
        if line.startswith('LOG 2'):
            continue
        # [thread id]: [start] [length] [name [perf]]
        g = pattern.match(line)
        if g != None:
          name = g.group(4)
          gg = cpattern.match(name)
          cache = 0
          if gg != None:
              name = gg.group(1)
              cache = int(gg.group(2))
          out.append({'name': name.strip(),
                      'procid': 0,
                      'threadid': int(g.group(1)),
                      'start': int(g.group(2)),
                      'length': int(g.group(3)),
                      'end': int(g.group(2)) + int(g.group(3)),
                      'cache': cache})
          continue

        # [node number] [thread id]: [start] [length] [name [perf]]
        g = mpipattern.match(line)
        if g != None:
          name = g.group(5)
          gg = cpattern.match(name)
          cache = 0
          if gg != None:
            name = gg.group(1)
            cache = int(gg.group(2))
          out.append({'name': name.strip(),
                      'procid': int(g.group(1)),
                      'threadid': int(g.group(2)),
                      'start': int(g.group(3)),
                      'length': int(g.group(4)),
                      'end': int(g.group(3)) + int(g.group(4)),
                      'cache': cache})
          continue

        # [thread id] [start] [length] [name]
        w = line.split()
        if len(w) == 4:
          out.append({'name': w[3],
                      'procid': 0,
                      'threadid': int(w[0]),
                      'start':  float(w[1])*1e6,
                      'length': float(w[2])*1e6,
                      'end': (float(w[1])+float(w[2]))*1e6,
                      'cache': 0})
          continue

        # parse error
        print "Error parsing line: ", line
    fh.close()
    return out

def getMedian(numericValues):
  theValues = sorted(numericValues)

  if len(theValues) % 2 == 1:
    return theValues[(len(theValues)+1)/2-1]
  else:
    lower = theValues[len(theValues)/2-1]
    upper = theValues[len(theValues)/2]

    return (float(lower + upper)) / 2


##################################################
# plot

import pylab

def drawBox(x0,x1,y0,y1,col):
    pylab.fill([x0, x1, x0], [y0, (y0+y1)/2, y1], fc=col[0], ec=col[1], linewidth=.5)

def drawText(x,y,text):
    pylab.text(x,y,text,horizontalalignment='center',verticalalignment='center',fontsize=taskfontsize)

def drawTask(x0,x1,y0,y1,orgtext):
    drawBox(x0,x1,y0,y1,getColor(orgtext))
    text = getText(orgtext)
    if len(text) == 2:
        drawText((x0+x1)/2,(y0+y1)/2 + barHeight/4,text[0])
        drawText((x0+x1)/2,(y0+y1)/2 - barHeight/4,text[1])
    elif len(text) == 3:
        drawText((x0+x1)/2,(y0+y1)/2 + barHeight/4,text[0])
        drawText((x0+x1)/2,(y0+y1)/2,text[1])
        drawText((x0+x1)/2,(y0+y1)/2 - barHeight/4,text[2])
    elif len(text) == 1:
        drawText((x0+x1)/2,(y0+y1)/2,text[0])
    extra = getExtra(orgtext)
    if len(extra) > 0:
        drawText(x0, y1+height*0.1, extra)

def drawPlot(tasks):
    height = 1.0
    barHeight = height * 0.8

    for task in tasks:
        x0 = task['start']
        x1 = x0 + task['length']
        y0 = task['threadid'] * height - barHeight / 2.0
        y1 = y0 + barHeight
        drawTask(x0, x1, y0, y1, task['name'])

    padding = barHeight/2
    pylab.ylim([ -barHeight/2 - padding, (numThreads-1)*height + barHeight/2 + padding]);
    yticks=range(0, numThreads)
    pylab.yticks(yticks, yticks);
    pylab.xlabel(r'Time')#,fontsize=labelfontsize)
    pylab.ylabel(r'Thread')#,fontsize=labelfontsize)


##################################################

filename = "schedule.dat"
if len(sys.argv) > 1:
    filename = sys.argv[1]

#taskfontsize = 16
#labelfontsize = 16
#tickfontsize = 16
timeScale = 1.0/1000000.0; # cycles -> Mcycles

tasks = load_file(filename)

# filter out uninteresting tasks

tasks = filter(myfilter, tasks)


# normalize start time

starttime = min([x['start'] for x in tasks])
for task in tasks:
  task['start'] = task['start']-starttime
  task['end'] = task['end']-starttime

# scale time

for task in tasks:
  task['start'] = task['start']*timeScale
  task['length'] = task['length']*timeScale
  task['end'] = task['end']*timeScale

# sort by processor (must be numbered properly)

procids = dict(zip([x['procid'] for x in tasks], [x['procid'] for x in tasks]))
numProcs = len(procids)
tasksperproc=[]
for i in sorted(procids):
  tasksperproc.append( [t for t in tasks if t['procid'] == i] )

# true threadids -> logical threadids

threadsPerProc = 0

for i in procids:
  ltasks = tasksperproc[i]
  threadids = dict(zip([x['threadid'] for x in ltasks], [x['threadid'] for x in ltasks]))
  numThreads = len(threadids)
  threadsPerProc = max(numThreads, threadsPerProc)
  threadidmap = dict(zip(sorted(threadids.keys()), range(0, len(threadids))))
  for task in ltasks:
    task['threadid'] = threadidmap[task['threadid']]

# calculate time wasted between tasks

mtd = list()

for p in procids:
  for i in range(0, numThreads):
    t = [ x for x in tasksperproc[p] if x['threadid'] == i ];
    for j in range(1, len(t)):
      mtd.append( t[j]['start']-(t[j-1]['start']+t[j-1]['length']) )

totaltime = sum([x['length'] for x in tasks])
endtime = max([x['end'] for x in tasks])
print 'N= ', len(tasks), \
      ' Total= ', totaltime, \
      ' End= ', endtime, \
      ' Par= ', "{0:.2f}".format(totaltime/float(endtime)), \
      ' DistMin= ', min(mtd), \
      ' DistMed= ', getMedian(mtd), \
      ' perf= ', sum([x['cache'] for x in tasks])

drawPlot(tasks)
pylab.show()
