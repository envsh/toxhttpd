package server

import (
	"bytes"
	"image"
	"net/http"
	"strings"
)

type MediaDataInfo struct {
	MimeType string `json:"mimetype"`
	Size     int    `json:"size"`
	Width    int    `json:"w,omitempty"`
	Height   int    `json:"h,omitempty"`
	Filename string
	MsgType  string
}

func getMediaDataInfo(data []byte, filename string) MediaDataInfo {
	info := MediaDataInfo{
		Size:     len(data),
		Filename: filename,
	}
	info.MimeType = http.DetectContentType(data)

	switch {
	case strings.HasPrefix(info.MimeType, "image/"):
		info.MsgType = "image"
	case strings.HasPrefix(info.MimeType, "video/"):
		info.MsgType = "video"
	case strings.HasPrefix(info.MimeType, "audio/"):
		info.MsgType = "audio"
	default:
		info.MsgType = "file"
	}

	if info.MsgType == "image" {
		cfg, _, err := image.DecodeConfig(bytes.NewReader(data))
		if err == nil {
			info.Width = cfg.Width
			info.Height = cfg.Height
		}
	}
	return info
}
