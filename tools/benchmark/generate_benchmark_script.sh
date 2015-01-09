#!/bin/bash

OUTPUT_DIR=./output

CONCURRENCIES=(   100    200    300    400    500    600    700    800    900   1000 )
NUMBERS=(      200000 200000 200000 200000 200000 200000 200000 200000 200000 200000 )
CONNECTIONS=('' '-k')
SERVER_NAMES=(apache nginx vertx h20 urbin)
SERVER_IPPORT=("127.0.0.1:80" "127.0.0.1:8080" "127.0.0.1:8000" "127.0.0.1:8088" "127.0.0.1:8888")
FILES=("/bench/file15bytes.html" "/bench/file28672bytes.bin")

echo "#!/bin/bash"
echo "rm -rf ${OUTPUT_DIR}/*.ab"
echo "date"
concurrency_i=0
for concurrency in ${CONCURRENCIES[@]}; do
	number=${NUMBERS[$concurrency_i]}
	for connection in "${CONNECTIONS[@]}"; do
		((server_i=0))
		for server_name in ${SERVER_NAMES[@]}; do
			server_ipport=${SERVER_IPPORT[$server_i]}
			for file in ${FILES[@]}; do
				fileName="${connection}_${number}_${concurrency}_${server_name}_${file}.ab"
				fileName=`echo ${fileName}|tr '/,' '__'`
				if [ "${server_name}" == "apache" -a "${concurrency}" -gt "500" ]; then
					cmd="touch ${OUTPUT_DIR}/${fileName}"
				else
					cmd="ab ${connection} -n ${number} -c ${concurrency} http://${server_ipport}${file} > ${OUTPUT_DIR}/${fileName}"
				fi
				echo $cmd
				#${cmd}
			done
			((server_i+=1))
		done
	done
	((concurrency_i+=1))
done
echo "date"
