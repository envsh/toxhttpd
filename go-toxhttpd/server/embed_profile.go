//go:build e1pack
package server

import (
	"log"
	_ "embed"
)

// copy from ../../data/savedata.bin

//go:embed savedata.bin
var embed_savedata []byte

func init() {
	getSaveData = func() []byte {
		return embed_savedata
	}
	log.Println("reseted getSaveData()")
}
