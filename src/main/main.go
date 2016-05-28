package main

import (
	"bufio"
	"io"
	"io/ioutil"
	"log"
	"os"
	//"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

const (
	syncPathFile string = `/home/onionhuang/programming_projects/golang/file_sync/sync_path.txt`
	maxThreads   int    = 8
)

var wg sync.WaitGroup

func main() {
	log.Println("Start file sync...")
	startTime := time.Now()

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

			// pass variable in outer area as arguments of gorutine function
			// in order to prevent from argument instability

			wg.Add(1)
			go func(sourcePath, destinationPath string) {
				syncFileDistribution(sourcePath, destinationPath)
				wg.Done()
			}(sourcePath, destinationPath)

			// sequential version
			// syncFileDistribution(sourcePath, destinationPath)
			// syncDestinationFile(destinationPath, sourcePath)

			wg.Add(1)
			go func(destinationPath, sourcePath string) {
				syncDestinationFile(destinationPath, sourcePath)
				wg.Done()
			}(destinationPath, sourcePath)
		}
	}
	wg.Wait()

	endTime := time.Now()
	processTime := endTime.Sub(startTime)

	log.Println("File sync done!")
	log.Printf("Processing time: %f seconds", processTime.Seconds())
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
					//	log.Printf("Directory does not exist in source: %q\n",
					//		filepath.Join(destinationPath, file.Name()))
					os.RemoveAll(recursiveDesPath)
				} else {

					// sequential version
					// syncDestinationFile(recursiveDesPath, recursiveSrcPath)

					// pass variable in outer area as arguments of gorutine function
					// in order to prevent from argument instability

					wg.Add(1)
					go func(recursiveDesPath, recursiveSrcPath string) {
						syncDestinationFile(recursiveDesPath, recursiveSrcPath)
						wg.Done()
					}(recursiveDesPath, recursiveSrcPath)
				}
			} else {
				if _, err := os.Stat(recursiveSrcPath); os.IsNotExist(err) {
					//	log.Printf("File does not exist in source: %q\n",
					//		filepath.Join(destinationPath, file.Name()))
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

				// sequential version
				// syncFileDistribution(recursiveSrcPath, recursiveDesPath)

				// pass variable in outer area as arguments of gorutine function
				// in order to prevent from argument instability

				wg.Add(1)
				go func(recursiveSrcPath, recursiveDesPath string) {
					syncFileDistribution(recursiveSrcPath, recursiveDesPath)
					wg.Done()
				}(recursiveSrcPath, recursiveDesPath)

			} else {
				srcFile := filepath.Join(sourcePath, file.Name())
				destFile := filepath.Join(destinationPath, file.Name())

				wg.Add(1)
				go func(srcFile, destFile string) {
					copyFile(srcFile, destFile)
					wg.Done()
				}(srcFile, destFile)
			}
		}
	}
}

func copyFile(src, dst string) {
	//	log.Printf("copy source: %q\n", src)
	//	log.Printf("copy destination: %q\n", dst)

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

		if fileDiff(src, dst) {
			//log.Printf("%s and %s are different\n", src, dst)
			copyFileContents(src, dst)
		}

		// compare the last modified time to determin whether to perform the copy operation

		//	srcFileMtime := sourceFileInfo.ModTime()
		//	desinationFileMtime := destFileInfo.ModTime()

		//	if srcFileMtime.After(desinationFileMtime) {
		//		copyFileContents(src, dst)
		//	}
	}
}

func fileDiff(src, dst string) bool {
	contentIn, err := ioutil.ReadFile(src)
	if err != nil {
		log.Fatalf("file read error: %s\n", src)
	}

	lenIn := len(contentIn)

	contentOut, err := ioutil.ReadFile(dst)
	if err != nil {
		log.Fatalf("file read error: %s\n", dst)
	}

	lenOut := len(contentOut)

	if lenIn != lenOut {
		return true
	}

	for i := 0; i < lenIn; i++ {
		cIn := contentIn[i]
		cOut := contentOut[i]

		if cIn != cOut {
			return true
		}
	}

	return false
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
