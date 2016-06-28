Copyright (C) 2016 Neil Edelman, see copying.txt.
neil dot edelman each mail dot mcgill dot ca

Version 1.0.

Usage: Penguin < novadata.tsv > allmisns.gv

The input file, eg, novadata.tsv, is misns and crons exported with EVNEW text
1.0.1; the output file, eg, allmisns.gv, is a GraphViz file; see
http://www.graphviz.org/. Then one could get a graph,

dot (or fdp, etc) allmisns.gv -O -Tpdf, or use the GUI.
