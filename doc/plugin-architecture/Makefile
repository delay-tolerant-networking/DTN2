.SUFFIXES: .eps .tex .ps .dvi .fig
SUPPORTTEX= plugin-architecture.tex bpa-interface.tex cla-interface.tex decision-interface.tex ds-interface.tex
SUPPORTFIG= figs/modules-orig.pdf figs/modules-rev.pdf figs/kbintf.pdf figs/components-toplevel.pdf
PDFDOCS= plugin-architecture.pdf

all: ${PDFDOCS} clean

%.pdf: %.tex ${SUPPORTTEX} ${SUPPORTFIG}
	pdflatex $<
	pdflatex $<
	pdflatex $<

%.eps: %.fig
	fig2dev -L eps $< $@

%.pdf: %.eps
	epstopdf $< > $@

clean:
	rm -f *.dvi *.log *.aux *.bbl *.blg *.toc *.lof *.lot 
realclean: clean
	rm -f *.ps *.pdf ${SUPPORTFIG}
