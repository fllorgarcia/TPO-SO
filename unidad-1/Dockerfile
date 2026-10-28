
FROM fedora:43

RUN dnf install -y gcc glibc-devel bash \
    && dnf clean all

WORKDIR /app

COPY auditor.c .

RUN gcc -std=c11 -O2 -Wall -Wextra \
    auditor.c -o /usr/local/bin/auditor

RUN mkdir -p /logs

ENV AUDIT_LOG=/logs/audit.log

WORKDIR /app

ENTRYPOINT ["/usr/local/bin/auditor"]
