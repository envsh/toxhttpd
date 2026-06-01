package server

import (
	"log"
	"strings"
	"math/rand"

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
		if ok, _ := t.AddTcpRelay(node.IPv4, node.Port, node.PublicKey); ok {
			extraRelays = append(extraRelays, node)
		}
		log.Printf("Bootstrap %d: TCP relay %s:%d (maintainer: %s) count=%d", i, node.IPv4, node.Port, node.Maintainer, len(extraRelays))
	}
}

var extraRelays []BootstrapNode

func (ctx *ApiContext) checkRebootstrap() {
	ctx.Rebscnter++
	if ctx.Rebscnter < 30*5 {
		return
	}
	ctx.Rebscnter = 0
	t := ctx.Tox
	connected := t.SelfGetConnectionStatus() != tox.CONNECTION_NONE
	if connected {
		return
	}
	if len(extraRelays) >= 5 {
		cnt := len(extraRelays)
		idx := int(rand.Uint32()/2)%cnt
		n := extraRelays[idx]
		t.Bootstrap(n.IPv4, n.Port, n.PublicKey)
		return
	}
	node, err := bsdata.SelectOne()
	if err != nil {
		return
	}
	if strings.Contains(node.Host, ":") { return }

	if ok, err2 := t.AddTcpRelay(node.Host, node.Ports[0], node.Pubkey); ok && err2 == nil {
		n := BootstrapNode{}
		n.IPv4  = node.Host
		n.Port  = node.Ports[0]
		n.PublicKey  = node.Pubkey
		n.Maintainer = node.Motd

		extraRelays = append(extraRelays, n)
		log.Printf("Relay added %d/5: %s", len(extraRelays), node.Host)
	}
	_, err1 := t.Bootstrap(node.Host, node.Ports[0], node.Pubkey)
	if err1 == nil {
		log.Println("Rebootstraped", node.Host)
	}
}
