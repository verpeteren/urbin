H1. RONJA

H1. Introduction

H2. Why

I want to have a webserver, that is fast, scriptable with javascript, that can talk with postgresql.

H2. How

Building upon the good ideas from others, glue it together.

H2. What

Something cool

H2. Who

Peter Reijnders <peter.reijnders@verpeteren.nl>

H1. Manual

H2. Install


./build.sh						# will create some platform specific settings, gets the dependencies and compiles

#
#cd ./src						#
#make -f Makefile.dependencies	# this may take some time
#make							# compiles
#../bin/ronja					# demo


H1. Project organisation

H2. Codestyle

Following ID software's code style guide: http://www.geeks3d.com/downloads/200811/idSoftware-Coding-Conventions-Geeks3D.com.pdf

H2. Dependencies

H3. Tools needed

These tools are needed to complie the dependencies to build, but after that, they are not needed anymore

sudo apt-get install autoconf2.13 wget git sed unzip build-essential g++ make cmake python2.7

H3. Integrated libraries

Topic		Project			License			SLOC		Link
Eventloop	picoev			BSD				 1140		http://developer.cybozu.co.jp/archives/kazuho/2009/08/picoev-a-tiny-e.html
Http Parser	H3				MIT				  481		https://github.com/c9s/h3
Regex		oniguruma		BSD				19458		http://www.geocities.jp/kosako3/oniguruma/
Postgresql	pq				Postgresql		53205		http://www.postgresql.org/docs/9.4/static/libpq.html
Portabilty	nspr			MPL2.0		   112077		https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR
Javascritp	spidermonkey	MPL2.0		   439718		https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey

H3. Optional libraries

Topic		Project			License			SLOC		Link
Mysql		mysac			GPL3			18593		http://cv.arpalert.org/page.sh?mysac

H3. Licence

TBD, probably MIT
MySql can be excluded from the linking


