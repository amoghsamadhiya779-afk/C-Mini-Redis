FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y g++ make
WORKDIR /app
COPY include/ ./include/
COPY src/ ./src/
COPY Makefile .
RUN make

FROM ubuntu:22.04 AS runner
WORKDIR /app
COPY --from=builder /app/mini_redis_server .
EXPOSE 6379
CMD ["./mini_redis_server"]