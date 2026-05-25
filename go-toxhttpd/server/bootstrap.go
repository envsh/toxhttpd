package server

import (
	"log"
	"strings"

	tox "github.com/TokTok/go-toxcore-c"
	"github.com/envsh/toxera/bsdata"
)

type BootstrapNode struct {
	IPv4       string
	IPv6       string
	Port       uint16
	PublicKey  string
	Maintainer string
}

var bootstrapNodes = []BootstrapNode{
	{
		IPv4:       "43.198.227.166",
		IPv6:       "-",
		Port:       3389,
		PublicKey:  "AD13AB0D434BCE6C83FE2649237183964AE3341D0AFB3BE1694B18505E4E135E",
		Maintainer: "AnthonyBilinski (C version)",
	},
	{
		IPv4:       "86.107.187.54",
		IPv6:       "-",
		Port:       33445,
		PublicKey:  "2C0F90965134C7BEFAFE72B077A19221628D7045BB51C1165A2C75CDB2B32634",
		Maintainer: "Boca",
	},
}

func bootstrapAll(t *tox.Tox) {
	for i, node := range bootstrapNodes {
		log.Printf("Bootstrap %d: UDP %s:%d (maintainer: %s)", i, node.IPv4, node.Port, node.Maintainer)
		t.Bootstrap(node.IPv4, node.Port, node.PublicKey)
		t.AddTcpRelay(node.IPv4, node.Port, node.PublicKey)
		log.Printf("Bootstrap %d: TCP relay %s:%d (maintainer: %s)", i, node.IPv4, node.Port, node.Maintainer)
	}
}

func (s *Server) checkRebootstrap() {
	s.rebscnter++
	if s.rebscnter < 20*5 {
		return
	}
	s.rebscnter = 0
	t := s.Tox
	connected := t.SelfGetConnectionStatus() != tox.CONNECTION_NONE
	if connected {
		return
	}
	node, err := bsdata.SelectOne()
	if err != nil {
		return
	}
	if strings.Contains(node.Host, ":") { return }

	_, err1 := t.Bootstrap(node.Host, node.Ports[0], node.Pubkey)
	_, err2 := t.AddTcpRelay(node.Host, node.Ports[0], node.Pubkey)
	log.Println("Rebootstraped", node.Host, err1, err2)
}
