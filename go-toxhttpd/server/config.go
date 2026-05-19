package server

import "flag"

type Config struct {
	UDPEnabled bool
	Port       string
	DebugLevel string
	WebRoot    string
}

func DefaultConfig() Config {
	return Config{
		UDPEnabled: false,
		Port:       "8181",
		DebugLevel: "info",
		WebRoot:    "",
	}
}

func ParseFlags() Config {
	cfg := DefaultConfig()
	flag.BoolVar(&cfg.UDPEnabled, "udp", cfg.UDPEnabled, "Enable UDP mode (default: TCP only)")
	flag.StringVar(&cfg.Port, "port", cfg.Port, "HTTP server port")
	flag.StringVar(&cfg.DebugLevel, "debug", cfg.DebugLevel, "Log level: trace, debug, info, warn, error")
	flag.Parse()
	return cfg
}
