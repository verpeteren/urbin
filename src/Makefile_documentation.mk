JAVA=java
JSRUNJAR=/usr/share/java/jsrun.jar
JSRUNJS=/usr/share/jsdoc-toolkit/app/run.js

CJS_DIR=$(GLOT_DIR)
TEMPLATE=$(DIR_DST)/docstrape_tmpl
FILES_SSJS=$(shell find $(CJS_DIR)/javascript*.* -type f )

BASE_URL=http://www.urbin.info
WIKI_LINK_ROOT=$(BASE_URL)/wiki
WIKI_URL=$(WIKI_LINK_ROOT)/
#WIKI_URL=$(WIKI_LINK_ROOT)/(?<comment>.*)
WIKI_TEXT=Go to the wiki for this item

allDocs: $(HTMLDOC_DIR)

$(HTMLDOC_DIR): $(FILES_SSJS) $(DIR_DST)
	@$(JAVA) -jar $(JSRUNJAR) $(JSRUNJS) $(FILES_SSJS) -t=$(TEMPLATE) -d=$(HTMLDOC_DIR)/ -x=cpp,c,h -n -s $(JSDOC_WEB_VARIABLES) --define="topic:server" --define='wikiurl:$(WIKI_URL)' --define='wikitext:$(WIKI_TEXT)' --define='base_url:./'          --define='wikiicon:http://upload.wikimedia.org/wikipedia/en/b/bc/Wiki.png'


