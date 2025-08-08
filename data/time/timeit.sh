#!/bin/bash

# Timing script for benchmarking executables against input files
# Usage: ./timeit.sh

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get all executables from bin directory
EXECUTABLES=($(ls bin/))

# Get all input files (without .obj extension)
INPUT_FILES=($(ls data/input/*.obj | sed 's|data/input/||' | sed 's|\.obj$||'))

echo -e "${GREEN}Found ${#EXECUTABLES[@]} executables: ${EXECUTABLES[*]}${NC}"
echo -e "${GREEN}Found ${#INPUT_FILES[@]} input files: ${INPUT_FILES[*]}${NC}"
echo

# Function to calculate average
calculate_average() {
    local values=("$@")
    local sum=0
    local count=${#values[@]}
    
    for value in "${values[@]}"; do
        sum=$(echo "$sum + $value" | bc -l)
    done
    
    echo "scale=3; $sum / $count" | bc -l
}

# Main timing loop
for input_file in "${INPUT_FILES[@]}"; do
    echo -e "${YELLOW}Processing input file: $input_file${NC}"
    
    # Create output directory if it doesn't exist
    mkdir -p "data/time/raw/$input_file"
    
    # Process each executable on this input file
    for executable in "${EXECUTABLES[@]}"; do
        echo -e "  ${YELLOW}Processing executable: $executable on $input_file${NC}"
        
        # Step 1: Warm up memory (4 times)
        echo -e "    Warming up memory..."
        for i in {1..4}; do
            /usr/bin/time -p ./bin/$executable data/input/$input_file.obj >/dev/null 2>&1
        done
        
        # Step 2: Run 8 times for data collection
        echo -e "    Collecting timing data..."
        
        real_times=()
        user_times=()
        sys_times=()
        
        # Function to validate timing data
        validate_time() {
            local time_val="$1"
            local run_num="$2"
            local time_type="$3"
            
            # Check for negative times
            if (( $(echo "$time_val < 0" | bc -l) )); then
                echo -e "      ${RED}Warning: Negative ${time_type} time in run $run_num: $time_val${NC}"
                return 1
            fi
            
            # Check for unreasonably large times (> 1 hour)
            if (( $(echo "$time_val > 3600" | bc -l) )); then
                echo -e "      ${RED}Warning: Unreasonably large ${time_type} time in run $run_num: $time_val${NC}"
                return 1
            fi
            
            return 0
        }
        
        # Function to check for large variations with outlier detection
        check_variation() {
            local current_time="$1"
            local time_type="$2"
            local run_num="$3"
            
            # Need at least 3 previous times to do outlier detection
            if [ ${#real_times[@]} -lt 3 ]; then
                return 0
            fi
            
            # Calculate median and median absolute deviation (MAD)
            local sorted_times=($(printf '%s\n' "${real_times[@]}" | sort -n))
            local n=${#sorted_times[@]}
            local median
            
            if [ $((n % 2)) -eq 0 ]; then
                # Even number of elements
                local mid1=$((n/2 - 1))
                local mid2=$((n/2))
                median=$(echo "scale=3; (${sorted_times[mid1]} + ${sorted_times[mid2]}) / 2" | bc -l)
            else
                # Odd number of elements
                local mid=$((n/2))
                median=${sorted_times[mid]}
            fi
            
            # Calculate MAD (median absolute deviation)
            local deviations=()
            for time in "${real_times[@]}"; do
                local dev=$(echo "scale=3; $time - $median" | bc -l | sed 's/^-//')
                deviations+=("$dev")
            done
            
            # Sort deviations and find median
            local sorted_deviations=($(printf '%s\n' "${deviations[@]}" | sort -n))
            local mad
            if [ $((n % 2)) -eq 0 ]; then
                local mid1=$((n/2 - 1))
                local mid2=$((n/2))
                mad=$(echo "scale=3; (${sorted_deviations[mid1]} + ${sorted_deviations[mid2]}) / 2" | bc -l)
            else
                local mid=$((n/2))
                mad=${sorted_deviations[mid]}
            fi
            
            # Check if current time is an outlier (more than 3.0 MAD from median - more reasonable)
            local current_dev=$(echo "scale=3; $current_time - $median" | bc -l | sed 's/^-//')
            local threshold=$(echo "scale=3; $mad * 3.0" | bc -l)
            
            # If MAD is 0 (all values identical), use a small percentage threshold
            if (( $(echo "$mad == 0" | bc -l) )); then
                threshold=$(echo "scale=3; $median * 0.01" | bc -l)  # 1% of median
            fi
            
            # Ensure minimum threshold of 5% of median to avoid rejecting reasonable variation
            local min_threshold=$(echo "scale=3; $median * 0.05" | bc -l)
            if (( $(echo "$threshold < $min_threshold" | bc -l) )); then
                threshold=$min_threshold
            fi
            
            if (( $(echo "$current_dev > $threshold" | bc -l) )); then
                echo -e "      ${RED}Warning: Outlier detected in ${time_type} time in run $run_num: current=$current_time, median=$median, mad=$mad, threshold=$threshold${NC}"
                return 1
            fi
            
            return 0
        }
        
        # Collect timing data with validation and retry
        runs_needed=8
        current_run=1
        
        while [ ${#real_times[@]} -lt $runs_needed ]; do
            # Use /usr/bin/time for more accurate timing
            time_output=$(/usr/bin/time -p ./bin/$executable data/input/$input_file.obj 2>&1)
            
            # Extract times from /usr/bin/time output
            real_time=$(echo "$time_output" | grep "^real" | awk '{print $2}')
            user_time=$(echo "$time_output" | grep "^user" | awk '{print $2}')
            sys_time=$(echo "$time_output" | grep "^sys" | awk '{print $2}')
            
            # Check for missing data
            if [ -z "$real_time" ] || [ -z "$user_time" ] || [ -z "$sys_time" ]; then
                echo -e "      ${RED}Warning: Missing time data in run $current_run${NC}"
                echo -e "      Raw time output: $time_output"
                current_run=$((current_run + 1))
                continue
            fi
            
            # Validate times
            valid_timing=true
            if ! validate_time "$real_time" "$current_run" "real"; then
                valid_timing=false
            fi
            if ! validate_time "$user_time" "$current_run" "user"; then
                valid_timing=false
            fi
            if ! validate_time "$sys_time" "$current_run" "sys"; then
                valid_timing=false
            fi
            
            # Check for large variations (only for real time)
            if ! check_variation "$real_time" "real" "$current_run"; then
                valid_timing=false
            fi
            
            # If timing is valid, store it
            if [ "$valid_timing" = true ]; then
                real_times+=("$real_time")
                user_times+=("$user_time")
                sys_times+=("$sys_time")
                echo -e "      Run $current_run: real=${real_time}s, user=${user_time}s, sys=${sys_time}s"
            else
                echo -e "      ${YELLOW}Rejecting run $current_run due to timing issues, will retry${NC}"
            fi
            
            current_run=$((current_run + 1))
        done
        
        # Step 3: Calculate averages
        echo -e "    Calculating averages..."
        real_avg=$(calculate_average "${real_times[@]}")
        user_avg=$(calculate_average "${user_times[@]}")
        sys_avg=$(calculate_average "${sys_times[@]}")
        
        # Step 4: Append averages to file in CSV format
        echo "$real_avg,$user_avg,$sys_avg" >> "data/time/raw/$input_file/$executable.txt"
        
        echo -e "    ${GREEN}Completed: $executable on $input_file${NC}"
        echo -e "    ${GREEN}Averages: real=${real_avg}s, user=${user_avg}s, sys=${sys_avg}s${NC}"
        echo
    done
    
    echo -e "${GREEN}Completed all executables for input file: $input_file${NC}"
    echo
done

echo -e "${GREEN}Timing script completed!${NC}"
echo -e "Results saved in data/time/raw/<input_file>/<executable>.txt"
