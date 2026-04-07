#pragma once
#include <Arduino.h>
#include <vector>

struct LinearOption {
  String id;
  String name;
};

struct LinearData {
  String teamId;
  String teamName;
  std::vector<LinearOption> states;
  std::vector<LinearOption> members;
  std::vector<LinearOption> projects;
  std::vector<LinearOption> labels;
};

struct IssueParams {
  String teamId;
  String title;
  String stateId;    // empty = Linear default
  int    priority;   // 0 = none, 1 = urgent … 4 = low
  String assigneeId; // empty = unassigned
  String projectId;  // empty = no project
  String labelId;    // empty = no label
};

struct IssueResult {
  bool   success;
  String identifier;  // e.g. "ACL-42"
  String error;
};

bool        linearFetchData(LinearData& data);
IssueResult linearCreateIssue(const IssueParams& params);
