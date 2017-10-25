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

python: manylinux1-pypi

test: \
	centos-7-test \
	debian-9-test \
	ubuntu-14.04-test \
	ubuntu-16.04-test \
	ubuntu-17.04-test

delivery:  archives python

# Ubuntu 14.04 images

ubuntu-14.04-image:
	docker build -f ubuntu-14.04.Dockerfile -t or-tools-ubuntu-14.04-image .

ubuntu-14.04-image-no-cache:
	docker build --no-cache -f ubuntu-14.04.Dockerfile -t or-tools-ubuntu-14.04-image .

ubuntu-14.04-archive: export ubuntu-14.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-14.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

ubuntu-14.04-test: export ubuntu-14.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-14.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

ubuntu-14.04-bash: export ubuntu-14.04-image
	docker run -it or-tools-ubuntu-14.04-image:latest /bin/bash

# Ubuntu 16.06 images

ubuntu-16.04-image:
	docker build -f ubuntu-16.04.Dockerfile -t or-tools-ubuntu-16.04-image .

ubuntu-16.04-image-no-cache:
	docker build --no-cache -f ubuntu-16.04.Dockerfile -t or-tools-ubuntu-16.04-image .

ubuntu-16.04-archive: export ubuntu-16.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

ubuntu-16.04-test: export ubuntu-16.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-16.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

ubuntu-16.04-bash: export ubuntu-16.04-image
	docker run -it or-tools-ubuntu-16.04-image:latest /bin/bash

# Ubuntu 17.04 images

ubuntu-17.04-image:
	docker build -f ubuntu-17.04.Dockerfile -t or-tools-ubuntu-17.04-image .

ubuntu-17.04-image-no-cache:
	docker build --no-cache -f ubuntu-17.04.Dockerfile -t or-tools-ubuntu-17.04-image .

ubuntu-17.04-archive: export ubuntu-17.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

ubuntu-17.04-test: export ubuntu-17.04-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-ubuntu-17.04-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

# Debian 9 images

debian-9-image:
	docker build -f debian-9.Dockerfile -t or-tools-debian-9-image .

debian-9-image-no-cache:
	docker build --no-cache -f debian-9.Dockerfile -t or-tools-debian-9-image .

debian-9-archive: export debian-9-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-debian-9-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

debian-9-test: export debian-9-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-debian-9-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

# Centos 7 images

centos-7-image:
	docker build -f centos-7.Dockerfile -t or-tools-centos-7-image .

centos-7-image-no-cache:
	docker build --no-cache -f centos-7.Dockerfile -t or-tools-centos-7-image .

centos-7-archive: export centos-7-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-centos-7-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test; make archive fz_archive; cp *.tar.gz /export"

centos-7-test: export centos-7-image
	docker run -w /root/or-tools -v `pwd`/export:/export or-tools-centos-7-image:latest /bin/bash -c "git pull; make clean; make all -j 5; make test"

# manylinux1 images

manylinux1-image:
	docker build -f manylinux1.Dockerfile -t or-tools-manylinux1-image .

manylinux1-image-no-cache:
	docker build --no-cache -f manylinux1.Dockerfile -t or-tools-manylinux1-image .

manylinux1-pypi: export manylinux1-image
	docker run -v `pwd`/export:/export or-tools-manylinux1-image:latest /bin/bash -c "/root/build/build-manylinux1.sh /root/src /root/build /export"
