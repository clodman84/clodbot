package main

import (
	"fmt"
	"io"
	"net"
)

func main() {
	socketPath := "/tmp/display_ctrl.sock"

	conn, err := net.Dial("unix", socketPath)
	if err != nil {
		fmt.Println("Dial error:", err)
		return
	}
	defer conn.Close()

	// Send data
	message := "toggle"
	_, err = conn.Write([]byte(message))
	if err != nil {
		fmt.Println("Write error:", err)
		return
	}

	// Read response
	buf := make([]byte, 1024)
	n, err := conn.Read(buf)
	if err != nil && err != io.EOF {
		fmt.Println("Read error:", err)
		return
	}

	fmt.Printf("Received: %s\n", string(buf[:n]))
}
