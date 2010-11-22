HTMLFLAGS = --book --duplex --no-numbered --format pdf14 --title --size letter

%.pdf: %.html
	-htmldoc $(HTMLFLAGS) --outfile $@ $<

all: dxpDoc.pdf doxygen mappingDocs install

doxygen:
	doxygen

mappingDocs:
	./makeMappingDocs

install:
	cp -f *.html /home/epics/web_software/epics
	cp -f *.png /home/epics/web_software/epics
	cp -f *.jpg /home/epics/web_software/epics
	cp -rfp dxpDoxygenHTML  /home/epics/web_software/epics/

clean:
	rm -rf dxpDoxygenHTML/*


realclean: clean
