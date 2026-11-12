# syntax=docker/dockerfile:1.7
# Commit image: the org C/C++ base plus the per-commit build. From the repo root:
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t warp-commit .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
# An ARG, not an ENV: an empty SOURCE_DATE_EPOCH breaks clang; build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH
# Declared without a default so the base's ASan+UBSan ENV flows through unless --build-arg overrides it.
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
