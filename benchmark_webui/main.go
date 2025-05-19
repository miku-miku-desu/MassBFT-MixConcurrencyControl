package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"gopkg.in/yaml.v3"
	"log"
	"net/http"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"sync"
	"time"
)

// StartRequest 前端请求的json数据结构
type StartRequest struct {
	Type       string `json:"type"`
	Throughput int    `json:"throughput"`
	Threads    int    `json:"threads"`
}

// State 后端给前端返回的json数据结构
type State struct {
	CommitRate         int     `json:"commitRate"`
	AbortRate          int     `json:"abortRate"`
	BenchmarkOutput    int     `json:"benchmarkOutput"`
	AvgLatency         int     `json:"avgLatency"`
	PendingTxs         int     `json:"pendingTxs"`
	AvgCommitRate      float64 `json:"avgCommitRate"`
	AvgAbortRate       float64 `json:"avgAbortRate"`
	AvgBenchmarkOutput float64 `json:"avgBenchmarkOutput"`
	AvgAvgLatency      float64 `json:"avgAvgLatency"`
	AvgPendingTxs      float64 `json:"avgPendingTxs"`
	IsFinished         bool    `json:"isFinished"`
}

var (
	states     []State    // state数组, 等待返回给前端
	stateMutex sync.Mutex // 互斥锁
	isRunning  bool
	cmd        *exec.Cmd // 运行子程序的实例
	// 平均值累加器
	totalCommitRate      int
	totalAbortRate       int
	totalBenchmarkOutput int
	totalAvgLatency      int
	totalPendingTxs      int
	stateCount           int

	// 用于获取yaml配置项
	type_map map[string]string = map[string]string{
		"ycsb":       "ycsb",
		"tpcc":       "tpcc",
		"crdt":       "crdt",
		"mix":        "mix_setting",
		"small_bank": "small_bank",
	}
)

func main() {
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "index.html")
	})

	http.HandleFunc("/api/start", startHandler)

	http.HandleFunc("/api/state", stateHandler)

	log.Println("Server starting on :8080...")
	log.Fatal(http.ListenAndServe(":8080", nil))
}

// 更新yaml配置文件
func updateConfig(benchmark string, throughput int, threads int) bool {
	data, err := os.ReadFile("peer.yaml")
	if err != nil {
		log.Fatalf("cannot read file peer.yaml")
		return false
	}

	var yamlMap map[string]interface{}
	err = yaml.Unmarshal(data, &yamlMap)
	if d, err := yamlMap[type_map[benchmark]].(map[string]interface{}); err {
		d["target_throughput"] = throughput
		d["thread_count"] = threads
	} else {
		log.Fatalf("unknown format yaml")
		return false
	}

	data, err = yaml.Marshal(yamlMap)
	if err != nil {
		log.Fatalf("cannot marshal yaml")
		return false
	}
	err = os.WriteFile("peer.yaml", data, 0644)
	if err != nil {
		log.Fatalf("cannot write file peer.yaml")
		return false
	}
	return true
}

func startHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		log.Printf("Method not allowed")
		return
	}

	var req StartRequest
	decoder := json.NewDecoder(r.Body)
	err := decoder.Decode(&req)
	if err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		log.Printf("Invalid JSON: %v", err)
		return
	}

	if !updateConfig(req.Type, req.Throughput, req.Threads) {
		http.Error(w, "Failed to update config", http.StatusInternalServerError)
		log.Printf("Failed to update config")
		return
	}

	stateMutex.Lock()
	if isRunning {
		stateMutex.Unlock()
		http.Error(w, "Benchmark is already running", http.StatusConflict)
		log.Printf("Benchmark is already running")
		return
	}
	isRunning = true
	states = nil // 清空状态数组
	// 重置平均值统计
	totalCommitRate = 0
	totalAbortRate = 0
	totalBenchmarkOutput = 0
	totalAvgLatency = 0
	totalPendingTxs = 0
	stateCount = 0
	stateMutex.Unlock()

	go runBenchmark(req)

	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]string{"status": "started"})
}

func stateHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		log.Printf("Method not allowed")
		return
	}

	stateMutex.Lock()
	defer stateMutex.Unlock()

	if len(states) == 0 {
		//if !isRunning {
		//	w.WriteHeader(http.StatusOK)
		//	json.NewEncoder(w).Encode(State{IsFinished: true})
		//	log.Printf("Benchmark is not running")
		//	return
		//}

		stateMutex.Unlock()
		// 等待一秒，如果还是没有state则认为已结束
		time.Sleep(1000 * time.Millisecond)
		stateMutex.Lock()
		if len(states) == 0 {
			w.WriteHeader(http.StatusOK)
			json.NewEncoder(w).Encode(State{IsFinished: true})
			return
		}
	}

	state := states[0]

	if len(states) == 1 && !isRunning {
		state.IsFinished = true
	}

	if len(states) > 0 {
		states = states[1:]
	}

	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(state)
}

// 启动benchmark程序并开始解析其输出
func runBenchmark(req StartRequest) {
	defer func() {
		stateMutex.Lock()
		isRunning = false
		stateMutex.Unlock()
	}()

	benchmarkPath := fmt.Sprintf("./%s", req.Type)
	cmd = exec.Command(benchmarkPath)

	stdout, err := cmd.StdoutPipe()
	if err != nil {
		log.Printf("Error creating stdout pipe: %v", err)
		return
	}
	stderr, err := cmd.StderrPipe()
	if err != nil {
		log.Printf("Error creating stderr pipe: %v", err)
		return
	}

	pipe := make(chan string)
	done := make(chan bool)

	// 分别处理stdout和stderr的输出
	go func() {
		scanner := bufio.NewScanner(stdout)
		for scanner.Scan() {
			line := scanner.Text()
			pipe <- line
		}
		done <- true
	}()

	go func() {
		scanner := bufio.NewScanner(stderr)
		for scanner.Scan() {
			line := scanner.Text()
			pipe <- line
		}
		done <- true
	}()

	err = cmd.Start()
	if err != nil {
		log.Printf("Error starting benchmark: %v", err)
		return
	}

	log.Printf("start %s success", req.Type)

Loop:
	for {
		select {
		case line := <-pipe:
			state, err := parseOutput(line)
			if err != nil {
				continue
			}

			stateMutex.Lock()
			states = append(states, state)
			stateMutex.Unlock()
		case <-done:
			log.Printf("benchmark %s done", req.Type)
			break Loop
		}
	}

	cmd.Wait()
}

// 解析输出, 使用正则表达式
func parseOutput(line string) (State, error) {

	var state State

	commitPattern := regexp.MustCompile(`commit: (\d+)`)
	abortPattern := regexp.MustCompile(`abort: (\d+)`)
	sendRatePattern := regexp.MustCompile(`send rate: (\d+)`)
	latencyPattern := regexp.MustCompile(`latency_ms: (\d+)`)
	pendingPattern := regexp.MustCompile(`pendingTx: (\d+)`)

	if match := commitPattern.FindStringSubmatch(line); len(match) > 1 {
		if val, err := strconv.Atoi(match[1]); err == nil {
			state.CommitRate = val
		}
	} else {
		return state, fmt.Errorf("invalid commit rate format")
	}

	if match := abortPattern.FindStringSubmatch(line); len(match) > 1 {
		if val, err := strconv.Atoi(match[1]); err == nil {
			state.AbortRate = val
		}
	} else {
		return state, fmt.Errorf("invalid abort rate format")
	}

	if match := sendRatePattern.FindStringSubmatch(line); len(match) > 1 {
		if val, err := strconv.Atoi(match[1]); err == nil {
			state.BenchmarkOutput = val
		}
	} else {
		return state, fmt.Errorf("invalid benchmark output format")
	}

	if match := latencyPattern.FindStringSubmatch(line); len(match) > 1 {
		if val, err := strconv.Atoi(match[1]); err == nil {
			state.AvgLatency = val
		}
	} else {
		return state, fmt.Errorf("invalid latency format")
	}

	if match := pendingPattern.FindStringSubmatch(line); len(match) > 1 {
		if val, err := strconv.Atoi(match[1]); err == nil {
			state.PendingTxs = val
		}
	} else {
		return state, fmt.Errorf("invalid pending transactions format")
	}

	log.Printf("%d %d %d %d %d", state.CommitRate, state.AbortRate, state.BenchmarkOutput, state.AvgLatency, state.PendingTxs)

	// 更新平均值
	stateMutex.Lock()
	stateCount++
	totalCommitRate += state.CommitRate
	totalAbortRate += state.AbortRate
	totalBenchmarkOutput += state.BenchmarkOutput
	totalAvgLatency += state.AvgLatency
	totalPendingTxs += state.PendingTxs

	state.AvgCommitRate = float64(totalCommitRate) / float64(stateCount)
	state.AvgAbortRate = float64(totalAbortRate) / float64(stateCount)
	state.AvgBenchmarkOutput = float64(totalBenchmarkOutput) / float64(stateCount)
	state.AvgAvgLatency = float64(totalAvgLatency) / float64(stateCount)
	state.AvgPendingTxs = float64(totalPendingTxs) / float64(stateCount)
	state.IsFinished = false
	stateMutex.Unlock()

	return state, nil
}
