FROM golang:1.26-alpine AS builder

WORKDIR /src
COPY go.mod ./
RUN go mod download
COPY *.go ./
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o /openliero-relay .

FROM scratch
COPY --from=builder /openliero-relay /openliero-relay
EXPOSE 19533/udp
EXPOSE 19600-19699/udp
ENTRYPOINT ["/openliero-relay"]
