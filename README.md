# Urbin

[The async'd non-block'd server](http://www.urbin.info)

![benchmark](http://www.urbin.info/static/img/screenshot_benchmarks.png "benchmark on a crappy laptop")

## Introduction

### Why

Let's face it. Web-apps are here to stay for the next decade, and we all want to be super-productive.

The browser side is javascript, using another language on the server causes too much thinking. So the serverside should be javascript as well.

I always felt that Node.js and vert.x were too magic and I do not want to land in mediocracy, so I do not want to follow the mainstream.

I want to have a special server, that is :

	* fast webserver
	* scriptable with javascript
	* that can talk with postgresql
	* works asyncronously
	* is blazing fast 
	* is super stabile
	* extendable with modules other then webserver, a sqlclient 

### How

Building upon the good ideas from others, glue it together.

	stability:	compile with full warnings / valgrind all the way
	Fast:		non blocking, async, zero copy
	Meme:		KEYMAKER: But like all systems, it has a weakness. The system is based on the rules of a building, one system built on another



### What

Something cool

### Who

Peter Reijnders <peter.reijnders@verpeteren.nl>

## Install


>	#Create some platform specific settings, gets the dependencies and compiles
>	`./build.sh

>	#Get and compile the dependencies (may take some time)

>	`cd ./src

>	`make deps`

>	#Then compile

>	`make all`

>	#Then configure

>	`cd ../bin`

>	`vi ./etc/urbin.conf`

>	#then hack

>	`vi ../var/scripts/javascript/main.urbin.js`

>	#Then run

>	`./urbin`

## Project organisation

### Codestyle


* Following ID software's code style guide: http://www.geeks3d.com/downloads/200811/idSoftware-Coding-Conventions-Geeks3D.com.pdf
* Take track of memory alloctions and the appropiate cleanup with an anonymous struct see http://blog.staila.com/?p=114 for details

### Dependencies

#### Tools needed

These tools are needed to compile the dependencies to build, but after that, they are not needed anymore. 

`sudo apt-get install autoconf2.13 wget git sed unzip build-essential g++ make cmake python2.7 head tail cat tac ar ranlib tr echo cut basename dirname echo strip jsdoc-toolkit`

#### Integrated libraries

|	Topic		|	Project			|	License			|	SLOC	|	Link	|
|---------------|-------------------|-------------------|-----------|-----------|
|	Eventloop	|	picoev			|	BSD				|	  1140	|	http://developer.cybozu.co.jp/archives/kazuho/2009/08/picoev-a-tiny-e.html	|
|	Http Parser	|	H3				|	MIT				|	   481	|	https://github.com/c9s/h3	|
|	Logging		|	c-logging		|	Public Domain	|	   230	|	https://github.com/dhess/c-logging	|
|	Dns			|	TADNS			|	Beer-ware		|	   570	|	http://adns.sourceforge.net/	|
|	Regex		|	oniguruma		|	BSD				|	 19458	|	http://www.geocities.jp/kosako3/oniguruma/	|
|	Postgresql	|	pq				|	Postgresql		|	 53205	|	http://www.postgresql.org/docs/9.4/static/libpq.html	|
|	Portabilty	|	nspr			|	MPL2.0			|	112077	|	https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR	|
|	Javascritp	|	spidermonkey	|	MPL2.0			|	439718	|	https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey	|

#### Optional libraries

>	Topic		Project			License			SLOC		Link
>	Mysql		mysac			GPL3			18593		http://cv.arpalert.org/page.sh?mysac

#### Disc space

To download and compile the dependecies the *deps* directory is a little bit over 3.5 Gb! Amazing..

### Licence

TBD, probably MIT

MySql can be excluded from the linking

## FAQ

Q:	**Why does this project use Makefiles?**

A:	They help with 'once-and-only-once' definitions

Q:	**Why do you download and compile all the dependencies yourself? You could use the system libraries.**

A:	As this is cutting edge, the dependencies have not landed in your distro yet, or are ancient.

Q:	**What is with the name?**

A:	Cool domain names are hard to get.

Q:	**What is the relationship with APE-Project?**

A:	Ajax-Push-Engine and libapenetwork are focussed on async sockets; This project strives to be less advanced (KISS) then APE. However some ideas appear to be simular.

Q:	**What is the relationship with H2O?**

A:	H2O is focussed on writing a very fast and complete HTTP1 and HTTP2 server. This project strives to be less advanced (KISS) then H2O. The project lead of H2O is the same genius that wrote the picoev event loop.

## Common errors

E:	**`error while loading shared libraries: libXXX.so: cannot open shared object file: No such file or directory`**

S: `export LD_LIBRARY_PATH=`pwd``

E: **`./etc/urbin.conf:2:` "`no such option`" or "`missing title for section`" or "`invalid integer value for option`"**

S: check that config file, in line 2: there is a typo
