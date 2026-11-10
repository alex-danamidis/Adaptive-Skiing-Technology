#include <iostream>
#include <cassert>
#include <vector>
#include "commands.h"

// unit test to check classifying and validating commands
int main() {
    // test classification
    std::vector<CommandPrediction> scores = {
        {"stop", 0.05f},
        {"left", 0.08f},
        {"noise", 0.02f},
        {"go", 0.80f},
        {"right", 0.04f},
        {"unknown", 0.01f}
    };

    CommandPrediction prediction = classifyCommand(scores);

    assert(prediction.label == "go");
    assert(prediction.confidence == 0.80f);
    assert(validateCommand(prediction) == Command::GO);

    // test low confidence
    std::vector<CommandPrediction> lowScores = {
        {"left", 0.30f},
        {"right", 0.40f},
        {"stop", 0.30f},
    };

    CommandPrediction lowPrediction = classifyCommand(lowScores);

    assert(lowPrediction.label == "right");
    assert(lowPrediction.confidence == 0.40f);
    assert(validateCommand(lowPrediction) == Command::INVALID);

    // test empty scores
    std::vector<CommandPrediction> emptyScores = {};

    CommandPrediction emptyPrediction = classifyCommand(emptyScores);
    
    assert(emptyPrediction.label == "");
    assert(emptyPrediction.confidence == 0.0f);
    assert(validateCommand(emptyPrediction) == Command::INVALID);

    // test valid commands
    assert(validateCommand({"go", 0.90f}) == Command::GO);
    assert(validateCommand({"stop", 0.90f}) == Command::STOP);
    assert(validateCommand({"left", 0.90f}) == Command::LEFT);
    assert(validateCommand({"right", 0.90f}) == Command::RIGHT);

    // test invalid commands
    assert(validateCommand({"noise", 0.90f}) == Command::INVALID);
    assert(validateCommand({"unknown", 0.90f}) == Command::INVALID);

    // test threshold (75% for now)
    assert(validateCommand({"left", 0.74f}) == Command::INVALID);
    assert(validateCommand({"left", 0.75f}) == Command::LEFT);

    std::cout << "All command tests passed!\n";
    return 0;
}