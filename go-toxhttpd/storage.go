package main

import (
	"database/sql"
	"log"
	"os"
	"path/filepath"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

// initMsgHistDB initializes SQLite database, creates pubkey_ids and events tables
// Sets events.rowid to start at 10000
func initMsgHistDB(dbPath string) (*sql.DB, error) {
	log.Printf("[DB] Initializing database at %s", dbPath)
	if err := os.MkdirAll(filepath.Dir(dbPath), 0700); err != nil {
		return nil, err
	}
	db, err := sql.Open("sqlite3", dbPath)
	if err != nil {
		return nil, err
	}

	// Check if new schema exists (check for pubkey_ids table)
	var tableName string
	err = db.QueryRow(`SELECT name FROM sqlite_master WHERE type='table' AND name='pubkey_ids'`).Scan(&tableName)
	needsMigration := (err != nil)
	
	if needsMigration {
		log.Printf("[DB] Migrating database to new schema")
		backupPath := dbPath + ".backup_" + time.Now().Format("20060102_150405")
		log.Printf("[DB] Backing up old database to %s", backupPath)
		// For development: drop old tables and recreate
		_, _ = db.Exec(`DROP TABLE IF EXISTS events`)
		_, _ = db.Exec(`DROP TABLE IF EXISTS sqlite_sequence`)
	}

	// Create pubkey_ids table
	log.Printf("[DB] Creating pubkey_ids table")
	_, err = db.Exec(`
		CREATE TABLE IF NOT EXISTS pubkey_ids (
			pkid   INTEGER PRIMARY KEY AUTOINCREMENT,
			pubkey TEXT NOT NULL UNIQUE
		)
	`)
	if err != nil {
		log.Printf("[DB] Error creating pubkey_ids: %v", err)
		db.Close()
		return nil, err
	}
	// Create unique index on pubkey
	_, err = db.Exec(`CREATE UNIQUE INDEX IF NOT EXISTS idx_pubkey_ids_pubkey ON pubkey_ids(pubkey)`)
	if err != nil {
		log.Printf("[DB] Error creating index: %v", err)
		db.Close()
		return nil, err
	}
	log.Printf("[DB] pubkey_ids table created successfully")

	// Create events table with INTEGER chanid
	log.Printf("[DB] Creating events table with INTEGER chanid")
	_, err = db.Exec(`
		CREATE TABLE IF NOT EXISTS events (
			rowid      INTEGER PRIMARY KEY AUTOINCREMENT,
			chanid     INTEGER NOT NULL,
			data       TEXT NOT NULL,
			created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
			FOREIGN KEY(chanid) REFERENCES pubkey_ids(pkid)
		)
	`)
	if err != nil {
		log.Printf("[DB] Error creating events table: %v", err)
		db.Close()
		return nil, err
	}
	// Create index on events.chanid
	_, err = db.Exec(`CREATE INDEX IF NOT EXISTS idx_events_chanid ON events(chanid)`)
	if err != nil {
		log.Printf("[DB] Error creating events index: %v", err)
		db.Close()
		return nil, err
	}
	log.Printf("[DB] events table created successfully")

	// Initialize sqlite_sequence for events table to start rowid at 10000
	log.Printf("[DB] Setting events.rowid to start at 10000")
	
	// Step 1: Insert a dummy record to trigger sqlite_sequence creation
	_, err = db.Exec(`INSERT INTO events(chanid, data) VALUES(0, '{"init":true}')`)
	if err != nil {
		log.Printf("[DB] Error inserting init record into events: %v", err)
		db.Close()
		return nil, err
	}
	
	// Step 2: Delete the dummy record
	_, err = db.Exec(`DELETE FROM events WHERE chanid=0`)
	if err != nil {
		log.Printf("[DB] Error deleting init record: %v", err)
		db.Close()
		return nil, err
	}
	
	// Step 3: Update sqlite_sequence to set seq=9999
	// Now sqlite_sequence should exist with an entry for 'events'
	result, err := db.Exec(`UPDATE sqlite_sequence SET seq=9999 WHERE name='events'`)
	if err != nil {
		log.Printf("[DB] Error updating sqlite_sequence: %v", err)
		db.Close()
		return nil, err
	}
	rowsAffected, _ := result.RowsAffected()
	if rowsAffected == 0 {
		// No row updated - maybe first time, insert the entry
		_, err = db.Exec(`INSERT INTO sqlite_sequence(name, seq) VALUES('events', 9999)`)
		if err != nil {
			log.Printf("[DB] Error inserting into sqlite_sequence: %v", err)
			db.Close()
			return nil, err
		}
	}
	
	// Verify the setting
	var seqVal int
	err = db.QueryRow(`SELECT seq FROM sqlite_sequence WHERE name='events'`).Scan(&seqVal)
	if err != nil {
		log.Printf("[DB] Warning: Could not verify sqlite_sequence: %v", err)
	} else {
		log.Printf("[DB] Verified: events sequence set to %d (next rowid will be %d)", seqVal, seqVal+1)
	}
	
	log.Printf("[DB] Database initialized successfully")
	return db, nil
}

// getOrCreatePubKeyID gets or creates pubkey to integer ID mapping
func (s *Server) getOrCreatePubKeyID(pubkey string) (int64, error) {
	var pkid int64
	err := s.db.QueryRow(`SELECT pkid FROM pubkey_ids WHERE pubkey = ?`, pubkey).Scan(&pkid)
	if err == nil {
		return pkid, nil
	}
	if err != sql.ErrNoRows {
		return 0, err
	}
	result, err := s.db.Exec(`INSERT INTO pubkey_ids(pubkey) VALUES(?)`, pubkey)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// persistEventToSQLite stores event to SQLite with integer chanid
func (s *Server) persistEventToSQLite(chanid int64, data string) {
	if _, err := s.db.Exec("INSERT INTO events(chanid, data) VALUES(?, ?)", chanid, data); err != nil {
		log.Printf("Failed to persist event to SQLite: %v", err)
	}
}
