FROM rockylinux:9

RUN dnf -y install gcc make glibc-devel procps-ng util-linux && \
    dnf clean all

WORKDIR /app

COPY . .

RUN make

CMD ["sleep", "infinity"]