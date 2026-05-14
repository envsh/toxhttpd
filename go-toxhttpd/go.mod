module github.com/envsh/toxhttpd

go 1.21.1

// replace github.com/TokTok/go-toxcore-c => ./go-toxcore-c

// replace github.com/envsh/toxera => ../../go-toxcore

require github.com/envsh/toxera v0.0.0-20260514083304-5303e11a7c20

require (
	github.com/TokTok/go-toxcore-c v0.2.18-0.20250216202442-0f7463080d5c
	github.com/kitech/touse/oai v0.0.0-20260513104332-2e22e5195b77
	github.com/mattn/go-sqlite3 v1.14.44
)

require (
	github.com/ebitengine/purego v0.10.0 // indirect
	github.com/petermattis/goid v0.0.0-20260330135022-df67b199bc81 // indirect
	github.com/sasha-s/go-deadlock v0.3.9 // indirect
	github.com/sashabaranov/go-openai v1.41.2 // indirect
	github.com/streamrail/concurrent-map v0.0.0-20160823150647-8bf1e9bacbf6 // indirect
)
