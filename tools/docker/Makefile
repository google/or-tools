# General commands

export:
	mkdir export

images: \
	ubuntu-14.04-image \
	ubuntu-16.04-image \
	ubuntu-17.04-image \
	centos-7-image \
	debian-9-image \
	manylinux1-image

images-no-cache: \
	ubuntu-14.04-image-no-cache \
	ubuntu-16.04-image-no-cache \
	ubuntu-17.04-image-no-cache \
	centos-7-image-no-cache \
	debian-9-image-no-cache

clean_all_images:
	docker rmi `docker images -q -a`

clean_all_containers:
	docker rm `docker ps -a -q`

archives: \
	centos-7-archive \
	debian-9-archive \
	ubuntu-14.04-archive \
	ubuntu-16.04-archive \
	ubuntu-17.04-archive

pypi: \
	centos-7-pypi \
	debian-9-pypi \
	ubuntu-14.04-pypi \
	ubuntu-16.04-pypi \
	ubuntu-17.04-pypi

pypi3: \
	debian-9-pypi3 \
	ubuntu-14.04-pypi3 \
	ubuntu-16.04-pypi3 \
	ubuntu-17.04-pypi3

test: \
	centos-7-test \
	debian-9-test \
	ubuntu-14.04-test \
	ubuntu-16.04-test \
	ubuntu-17.04-test

delivery:  archives pypi pypi3

# Ubuntu 14.04 images

ubuntu-14.04-image:
	docker build -f ubuntu-14.04.Dockerfile -t or-tools-ubuntu-14.04-image .

ubuntu-14.04-image-no-cache:
	docker build --no-cache -f ubuntu-14.04.Dockerfile -t or-tools-ubuntu-14.04-image .

ubuntu-14.04-archive: export ubuntu-14.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-14.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

ubuntu-14.04-test: export ubuntu-14.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-14.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

ubuntu-14.04-pypi: export ubuntu-14.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-14.04-image:latest /bin/bash -c "git pull; make clean; make python -j 5; make test_python; PYPI_OS=-ubuntu-14.04 make pypi_export python_examples_archive; cp *.tar.gz /export"

ubuntu-14.04-pypi3: export ubuntu-14.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-14.04-image:latest /bin/bash -c "git pull; sed -i -e 's/2.7/3.4/g' Makefile.local; make clean; make python -j 5; make test_python; PYPI_OS=-ubuntu-14.04 make pypi_export"

# Ubuntu 16.06 images

ubuntu-16.04-image:
	docker build -f ubuntu-16.04.Dockerfile -t or-tools-ubuntu-16.04-image .

ubuntu-16.04-image-no-cache:
	docker build --no-cache -f ubuntu-16.04.Dockerfile -t or-tools-ubuntu-16.04-image .

ubuntu-16.04-archive: export ubuntu-16.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

ubuntu-16.04-test: export ubuntu-16.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

ubuntu-16.04-pypi: export ubuntu-16.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; make clean; make python -j 5; make test_python; PYPI_OS=-ubuntu-16.04 make pypi_export python_examples_archive; cp *.tar.gz /export"

ubuntu-16.04-pypi3: export ubuntu-16.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; sed -i -e 's/2.7/3.5/g' Makefile.local; make clean; make python -j 5; make test_python; PYPI_OS=-ubuntu-16.04 make pypi_export"

# Ubuntu 17.04 images

ubuntu-17.04-image:
	docker build -f ubuntu-17.04.Dockerfile -t or-tools-ubuntu-17.04-image .

ubuntu-17.04-image-no-cache:
	docker build --no-cache -f ubuntu-17.04.Dockerfile -t or-tools-ubuntu-17.04-image .

ubuntu-17.04-archive: export ubuntu-17.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

ubuntu-17.04-test: export ubuntu-17.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

ubuntu-17.04-pypi: export ubuntu-17.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; make clean; make python -j 5; make test_python; PYPI_OS=-ubuntu-17.04 make pypi_export python_examples_archive; cp *.tar.gz /export"

ubuntu-17.04-pypi3: export ubuntu-17.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; sed -i -e 's/2.7/3.5/g' Makefile.local; make clean; make python -j 5; make test_python; PYPI_OS=-ubuntu-17.04 make pypi_export"

# Debian 9 images

debian-9-image:
	docker build -f debian-9.Dockerfile -t or-tools-debian-9-image .

debian-9-image-no-cache:
	docker build --no-cache -f debian-9.Dockerfile -t or-tools-debian-9-image .

debian-9-archive: export debian-9-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-debian-9-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

debian-9-test: export debian-9-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-debian-9-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

debian-9-pypi: export debian-9-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-debian-9-image:latest /bin/bash -c "git pull; make clean; make python -j 5; make test_python; PYPI_OS=-debian-9 make pypi_export python_examples_archive; cp *.tar.gz /export"

debian-9-pypi3: export debian-9-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-debian-9-image:latest /bin/bash -c "git pull; sed -i -e 's/2.7/3.5/g' Makefile.local; make clean; make python -j 5; make test_python; PYPI_OS=-debian-9 make pypi_export"

# Centos 7 images

centos-7-image:
	docker build -f centos-7.Dockerfile -t or-tools-centos-7-image .

centos-7-image-no-cache:
	docker build --no-cache -f centos-7.Dockerfile -t or-tools-centos-7-image .

centos-7-archive: export centos-7-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-centos-7-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

centos-7-test: export centos-7-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-centos-7-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

centos-7-pypi: export centos-7-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-centos-7-image:latest /bin/bash -c "git pull; make clean; make python -j 5; make test_python; PYPI_OS=-centos-7 make pypi_export python_examples_archive; cp *.tar.gz /export"

# manylinux1 images

manylinux1-image:
	docker build -f manylinux1.Dockerfile -t or-tools-manylinux1-image .

manylinux1-pypi3: export manylinux1-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-manylinux1-image:latest /bin/bash -c "/root/build-manylinux1.sh /root /export"
