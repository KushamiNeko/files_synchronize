package main

import (
	"bytes"
	"io"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sync"
	"time"
)

const (
	SYNCFILEPATH string = `/home/onionhuang/programming_projects/golang/file_sync/sync_path.txt`
)

var wg sync.WaitGroup

func main() {
	runtime.GOMAXPROCS(runtime.NumCPU())

	log.Println("Start file sync...")
	startTime := time.Now()

	fileContent, err := ioutil.ReadFile(SYNCFILEPATH)
	if err != nil {
		log.Fatalf("Cannot open the file: %q\n", SYNCFILEPATH)
	}

	if bytes.Contains(fileContent, []byte("\ufeff")) {
		fileContent = bytes.Replace(fileContent, []byte("\ufeff"), []byte(""), -1)
	}

	pathSplit := bytes.Split(fileContent, []byte("\n"))

	for _, i := range pathSplit {
		path := bytes.TrimSpace(i)

		if len(path) == 0 {
			continue
		}

		rePattern, err := regexp.Compile(`^(.+) -> (.+)$`)
		if err != nil {
			log.Fatalf("regex pattern compile failed\n")
		}

		pathMatch := rePattern.FindSubmatch(path)

		srcPath := filepath.Clean(string(bytes.TrimSpace(pathMatch[1])))
		desPath := filepath.Clean(string(bytes.TrimSpace(pathMatch[2])))

		if _, err := os.Stat(srcPath); os.IsNotExist(err) {
			log.Printf("Source path does not exist: %q\n", srcPath)
			continue
		}

		if _, err := os.Stat(desPath); os.IsNotExist(err) {
			log.Printf("destinationPath does not exist: %q\n",
				desPath)
			continue
		}

		//	log.Println(srcPath)
		//	log.Println(desPath)

		// pass variable in outer area as arguments of gorutine function
		// in order to prevent from argument instability

		wg.Add(1)
		go func(sourcePath, destinationPath string) {
			syncSrcToDest(sourcePath, destinationPath)
			wg.Done()
		}(srcPath, desPath)

		//		// sequential version
		//		// syncFileDistribution(sourcePath, destinationPath)
		//		// syncDestinationFile(destinationPath, sourcePath)

		wg.Add(1)
		go func(destinationPath, sourcePath string) {
			syncDestToSrc(destinationPath, sourcePath)
			wg.Done()
		}(desPath, srcPath)

	}

	wg.Wait()

	endTime := time.Now()
	processTime := endTime.Sub(startTime)

	log.Println("File sync done!")
	log.Printf("Processing time: %f seconds", processTime.Seconds())
}

func syncDestToSrc(desPath, srcPath string) {

	destFileInfo, err := os.Stat(desPath)
	if err != nil {
		log.Printf("Fatal error on destination file: %q\n", destFileInfo.Name())
		return
	}

	if destFileInfo.IsDir() {
		destRead, err := ioutil.ReadDir(desPath)
		if err != nil {
			log.Printf(
				"Fatal error occurs while walk through destination path: \n%q\n",
				desPath)
			return
		}

		for _, file := range destRead {
			recursiveSrcPath := filepath.Join(srcPath, file.Name())
			recursiveDesPath := filepath.Join(desPath, file.Name())
			if file.IsDir() {
				if _, err := os.Stat(recursiveSrcPath); os.IsNotExist(err) {
					os.RemoveAll(recursiveDesPath)
				} else {

					// sequential version
					// syncDestinationFile(recursiveDesPath, recursiveSrcPath)

					// pass variable in outer area as arguments of gorutine function
					// in order to prevent from argument instability

					wg.Add(1)
					go func(recursiveDesPath, recursiveSrcPath string) {
						syncDestToSrc(recursiveDesPath, recursiveSrcPath)
						wg.Done()
					}(recursiveDesPath, recursiveSrcPath)
				}
			} else {
				if _, err := os.Stat(recursiveSrcPath); os.IsNotExist(err) {
					os.Remove(recursiveDesPath)
				}
			}
		}
	}
}

func syncSrcToDest(srcPath, desPath string) {

	sourceFileInfo, err := os.Stat(srcPath)
	if err != nil {
		log.Printf("Fatal error on source file: %q\n", sourceFileInfo.Name())
		return
	}

	if sourceFileInfo.IsDir() {
		if _, err := os.Stat(desPath); os.IsNotExist(err) {
			err := os.MkdirAll(desPath, 0777)
			if err != nil {
				log.Printf("Directories creation errors: \n%q\n", desPath)
				return
			}
		}

		sourcePathRead, err := ioutil.ReadDir(srcPath)
		if err != nil {
			log.Printf(
				"Fatal error occurs while walk through source path: \n%q\n",
				srcPath)
			return
		}

		for _, file := range sourcePathRead {
			if file.IsDir() {
				recursiveSrcPath := filepath.Join(srcPath, file.Name())
				recursiveDesPath := filepath.Join(desPath, file.Name())

				//log.Println(recursiveSrcPath)
				//log.Println(recursiveDesPath)

				// sequential version
				//syncSrcToDest(recursiveSrcPath, recursiveDesPath)

				// pass variable in outer area as arguments of gorutine function
				// in order to prevent from argument instability

				wg.Add(1)
				go func(recursiveSrcPath, recursiveDesPath string) {
					syncSrcToDest(recursiveSrcPath, recursiveDesPath)
					wg.Done()
				}(recursiveSrcPath, recursiveDesPath)

			} else {
				srcFile := filepath.Join(srcPath, file.Name())
				destFile := filepath.Join(desPath, file.Name())

				copyFile(srcFile, destFile)
			}
		}
	}
}

func copyFile(src, dst string) {

	//log.Printf("fileName: %q\n", src)
	sourceFileInfo, err := os.Stat(filepath.Clean(src))
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

		//	if fileDiff(src, dst) {
		//		copyFileContents(src, dst)
		//	}

		//compare the last modified time to determin whether to perform the copy operation

		srcFileMtime := sourceFileInfo.ModTime()
		desinationFileMtime := destFileInfo.ModTime()

		if srcFileMtime.After(desinationFileMtime) {
			copyFileContents(src, dst)
		}
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
