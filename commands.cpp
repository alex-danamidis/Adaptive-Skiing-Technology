#include "commands.h"

// function to find the highest confidence prediction
CommandPrediction classifyCommand(const std::vector<CommandPrediction>& scores) {
    CommandPrediction prediction = {"", 0.0f};

    for(const auto& score : scores) {
        if(score.confidence > prediction.confidence) {
            prediction.confidence = score.confidence;
            prediction.label = score.label;
        }
    }

    return prediction;
}

// function to validate/standardize the prediction
Command validateCommand(const CommandPrediction& prediction) {
    const float confidence_threshold = 0.75; // tentative threshold, can adjust as model gets better

    if (prediction.confidence < confidence_threshold) {
        return Command::INVALID;
    }

    if (prediction.label == "left") {
        return Command::LEFT;
    } else if (prediction.label == "right") {
        return Command::RIGHT;
    } else if (prediction.label == "stop") {
        return Command::STOP;
    } else if (prediction.label == "go") {
        return Command::GO;
    }

    return Command::INVALID;
}