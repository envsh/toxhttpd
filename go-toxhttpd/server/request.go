package server

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strconv"
	"strings"
)

type RequestParams struct {
	formValues url.Values
	jsonData   map[string]interface{}
	isJSON     bool
}

func getRequestParams(r *http.Request) (*RequestParams, error) {
	params := &RequestParams{}
	contentType := r.Header.Get("Content-Type")

	if strings.Contains(contentType, "application/json") {
		params.isJSON = true
		params.jsonData = make(map[string]interface{})
		if err := json.NewDecoder(r.Body).Decode(&params.jsonData); err != nil {
			return nil, fmt.Errorf("failed to parse JSON: %v", err)
		}
	} else {
		if err := r.ParseForm(); err != nil {
			return nil, fmt.Errorf("failed to parse form: %v", err)
		}
		params.formValues = r.Form
	}
	return params, nil
}

func (p *RequestParams) Get(key string) string {
	if p.isJSON {
		val, ok := p.jsonData[key]
		if !ok {
			return ""
		}
		switch v := val.(type) {
		case string:
			return v
		case float64:
			return strconv.FormatFloat(v, 'f', -1, 64)
		default:
			return fmt.Sprintf("%v", v)
		}
	}
	return p.formValues.Get(key)
}
