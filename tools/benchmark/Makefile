DATASET=/var/www/www.urbin.info/static/js/benchmark_dataset.js
VARWWW=./var_www
FILE15=$(VARWWW)/file15bytes.html
FILE28672=$(VARWWW)/file28672bytes.bin

all:	$(DATASET)

runbench.sh:	generate_benchmark_script.sh 
	./$< > $@

./output: runbench.sh $(FILE15) $(FILE28672)
	mkdir -p output
	$<

$(DATASET): analyse_ab_benchmarks.py output
	$^ > $@

$(VARWWW):
	mkdir $@

$(FILE15): $(VARWWW)
	echo '<h1>hello</h1>' > $@

$(FILE28672): $(VARWWW)
	dd if=/dev/urandom of=$@ bs=1K count=28
