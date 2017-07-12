export:
	mkdir export

ubuntu16.04-image:
	docker build -f ubuntu-16.04.Dockerfile -t or-tools-ubuntu-16.04-image .

ubuntu16.04-archive: export
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive python_examples_archive; cp *.tar.gz /export"

ubuntu17.04-image:
	docker build -f ubuntu-17.04.Dockerfile -t or-tools-ubuntu-17.04-image .

ubuntu17.04-archive: export
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; pwd; make clean; make all -j 5; make test; make archive; cp *.tar.gz /export"
