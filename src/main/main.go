package main

import (
	"bufio"
	"io"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"strings"
)

const (
	syncPathFile string = `/home/onionhuang/programming_projects/golang/file_sync/sync_path.txt`
	maxThreads   int    = 8
)

func main() {
	fn, err := os.Open(syncPathFile)
	if err != nil {
		log.Fatalf("%q\n", "Cannot open the file!")
	}
	defer fn.Close()

	fnReader := bufio.NewReader(fn)
	contents, err := fnReader.ReadString('}')
	if err != nil {
		log.Printf("reading error: %q\n", err)
		return
	}

	// chanSource := make(chan string, maxThreads)
	// chanDestination := make(chan string, maxThreads)
	chanSource := make(chan [2]string, maxThreads)
	// chanDestination := make(chan [2]string, maxThreads)
	chanSourceDone := make(chan bool, maxThreads)
	// chanDestinationDone := make(chan bool, maxThreads)
	threadCounts := 0

	for i := 0; i < maxThreads; i++ {
		go func(source chan [2]string, done chan bool) {
			for {
				// sourcePath := <-source
				// destinationPath := <-destination
				source := <-source
				sourcePath := source[0]
				destinationPath := source[1]
				// log.Printf("Source: %q\n", sourcePath)
				// log.Printf("Destination: %q\n", destinationPath)
				syncFileDistribution(sourcePath, destinationPath)
				syncDestinationFile(destinationPath, sourcePath)
				done <- true
			}
		}(chanSource, chanSourceDone)
	}

	pathClean := strings.Replace(contents, "{", "", -1)
	pathClean = strings.Replace(pathClean, "}", "", -1)
	pathGroup := strings.Split(pathClean, ";")
	for _, i := range pathGroup {
		pathClean := strings.TrimSpace(i)
		if pathClean != "" {
			pathInfo := strings.Split(pathClean, ":")
			sourcePath := strings.TrimSpace(pathInfo[0])
			destinationPath := strings.TrimSpace(pathInfo[1])

			if _, err := os.Stat(sourcePath); os.IsNotExist(err) {
				log.Printf("Source path does not exist: %q\n", sourcePath)
				continue
			}

			if _, err := os.Stat(destinationPath); os.IsNotExist(err) {
				log.Printf("destinationPath does not exist: %q\n",
					destinationPath)
				continue
			}

			// chanSource <- sourcePath
			// chanDestination <- destinationPath
			source := [2]string{sourcePath, destinationPath}
			chanSource <- source
			// chanDestination <- source
			threadCounts++
			// syncFileDistribution(sourcePath, destinationPath)
			// syncDestinationFile(destinationPath, sourcePath)
		}

		if threadCounts > maxThreads {
			<-chanSourceDone
			// <-chanDestinationDone
			threadCounts--
		}
	}

	for i := 0; i < maxThreads; i++ {
		<-chanSourceDone
		// <-chanDestinationDone
		threadCounts--
		if threadCounts <= 0 {
			return
		}
	}
}

func syncDestinationFile(desPath, srcPath string) {
	sourcePath := filepath.Clean(srcPath)
	destinationPath := filepath.Clean(desPath)

	destFileInfo, err := os.Stat(destinationPath)
	if err != nil {
		log.Printf("Fatal error on source file: %q\n", destFileInfo.Name())
		return
	}

	if destFileInfo.IsDir() {
		destRead, err := ioutil.ReadDir(destinationPath)
		if err != nil {
			log.Printf(
				"Fatal error occurs while walk through source path: %q\n",
				filepath.Base(desPath))
			return
		}

		for _, file := range destRead {
			recursiveSrcPath := filepath.Join(sourcePath, file.Name())
			recursiveDesPath := filepath.Join(destinationPath, file.Name())
			if file.IsDir() {
				if _, err := os.Stat(recursiveSrcPath); os.IsNotExist(err) {
					log.Printf("Directory does not exist in source: %q\n",
						filepath.Join(destinationPath, file.Name()))
					os.RemoveAll(recursiveDesPath)
				} else {
					// recSource := [2]string{recursiveSrcPath, recursiveDesPath}
					// cSource <- recSource
					syncDestinationFile(recursiveDesPath, recursiveSrcPath)
				}
			} else {
				if _, err := os.Stat(recursiveSrcPath); os.IsNotExist(err) {
					log.Printf("File does not exist in source: %q\n",
						filepath.Join(destinationPath, file.Name()))
					os.Remove(recursiveDesPath)
				}
			}
		}
	}
}

func syncFileDistribution(srcPath, desPath string) {
	sourcePath := filepath.Clean(srcPath)
	destinationPath := filepath.Clean(desPath)

	sourceFileInfo, err := os.Stat(sourcePath)
	if err != nil {
		log.Printf("Fatal error on source file: %q\n", sourceFileInfo.Name())
		return
	}

	if sourceFileInfo.IsDir() {
		if _, err := os.Stat(destinationPath); os.IsNotExist(err) {
			err := os.MkdirAll(destinationPath, 0777)
			if err != nil {
				log.Printf("Directories creation errors")
				return
			}
		}
		sourcePathRead, err := ioutil.ReadDir(sourcePath)
		if err != nil {
			log.Printf(
				"Fatal error occurs while walk through source path: %q\n",
				filepath.Base(srcPath))
			return
		}

		for _, file := range sourcePathRead {
			if file.IsDir() {
				recursiveSrcPath := filepath.Join(sourcePath, file.Name())
				recursiveDesPath := filepath.Join(destinationPath, file.Name())
				// recSource := [2]string{recursiveSrcPath, recursiveDesPath}
				// cSource <- recSource
				syncFileDistribution(recursiveSrcPath, recursiveDesPath)
			} else {
				srcFile := filepath.Join(sourcePath, file.Name())
				destFile := filepath.Join(destinationPath, file.Name())
				copyFile(srcFile, destFile)
			}
		}
	}
}

func copyFile(src, dst string) {
	log.Printf("copy source: %q\n", src)
	log.Printf("copy destination: %q\n", dst)

	sourceFileInfo, err := os.Stat(src)
	if err != nil {
		log.Printf("Copying error on source file: %q\n", sourceFileInfo.Name())
		return
	}

	if !(sourceFileInfo.Mode().IsRegular()) {
		log.Printf("Non-regular source file: %q", sourceFileInfo.Name())
		return
	}

	if _, err := os.Stat(dst); os.IsNotExist(err) {
		copyFileContents(src, dst)
	} else {
		destFileInfo, err := os.Stat(dst)
		if err != nil {
			log.Printf("Copying error on destination file: %q\n",
				destFileInfo.Name())
			return
		}

		if !(destFileInfo.Mode().IsRegular()) {
			log.Printf("Non-regular destination file: %q", destFileInfo.Name())
			return
		}

		srcFileMtime := sourceFileInfo.ModTime()
		desinationFileMtime := destFileInfo.ModTime()

		if srcFileMtime.After(desinationFileMtime) {
			copyFileContents(src, dst)
		}
	}
}

func copyFileContents(src, dst string) {
	in, err := os.Open(src)
	if err != nil {
		return
	}
	defer in.Close()

	out, err := os.Create(dst)
	if err != nil {
		return
	}
	defer out.Close()

	if _, err := io.Copy(out, in); err != nil {
		return
	}

	err = out.Sync()
	return
}