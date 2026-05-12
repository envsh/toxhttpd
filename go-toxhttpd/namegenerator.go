package main

import "math/rand"

func randomName() string {
	adj := []string{"Happy", "Lucky", "Swift", "Bold", "Cool", "Wild", "Bright", "Calm", "Quick", "Smart", "Brave", "Nice", "Fast", "Kind", "Neat", "Warm", "Soft", "Pure", "True", "Free", "Fierce", "Gentle", "Jolly", "Mighty", "Merry", "Proud", "Sharp", "Sleek", "Sly", "Zesty"}
	noun := []string{"Panda", "Fox", "Eagle", "Tiger", "Otter", "Hawk", "Lynx", "Wolf", "Bear", "Deer", "Owl", "Frog", "Crab", "Seal", "Dove", "Wren", "Elk", "Hare", "Moth", "Finch", "Badger", "Crane", "Gecko", "Heron", "Koala", "Mole", "Oryx", "Sloth", "Toad", "Viper"}
	return adj[rand.Intn(len(adj))] + " " + noun[rand.Intn(len(noun))]
}
