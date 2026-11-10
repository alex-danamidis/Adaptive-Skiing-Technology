#pragma once

#include <string>
#include <vector>

// supported commands
enum class Command {
    LEFT,
    RIGHT,
    STOP,
    GO,
    INVALID
};

// highest confidence prediction from the model
struct CommandPrediction {
    std::string label;
    float confidence;
};

// function to find the highest confidence prediction
CommandPrediction classifyCommand(const std::vector<CommandPrediction>& scores);

// function to validate/standardize the prediction
Command validateCommand(const CommandPrediction& prediction);