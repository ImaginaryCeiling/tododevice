#include "linear.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// GraphQL queries
// ---------------------------------------------------------------------------

static const char BOOT_QUERY[] PROGMEM = R"({
  teams {
    nodes {
      id name key
      states(first:50) { nodes { id name type } }
      members(first:50) { nodes { id name } }
      labels(first:50) { nodes { id name } }
    }
  }
  projects(first:50) { nodes { id name } }
})";

static const char CREATE_MUTATION[] PROGMEM = R"(
mutation($input:IssueCreateInput!){
  issueCreate(input:$input){
    success
    issue { id identifier title }
  }
})";

// ---------------------------------------------------------------------------
// HTTP helper
// ---------------------------------------------------------------------------

static String post(const String& body) {
  WiFiClientSecure client;
  client.setInsecure();  // skip cert verification for now

  HTTPClient http;
  http.begin(client, LINEAR_API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", LINEAR_API_KEY);
  http.setTimeout(10000);

  int code = http.POST(body);
  String resp;
  if (code > 0) {
    resp = http.getString();
  } else {
    Serial.printf("HTTP error: %d\n", code);
  }
  http.end();
  return resp;
}

// ---------------------------------------------------------------------------
// Fetch teams, states, members, projects, labels
// ---------------------------------------------------------------------------

bool linearFetchData(LinearData& data) {
  // Build request
  DynamicJsonDocument req(512);
  req["query"] = BOOT_QUERY;
  String body;
  serializeJson(req, body);

  Serial.println("Fetching Linear data...");
  String resp = post(body);
  if (resp.isEmpty()) return false;

  // Parse — response can be large
  DynamicJsonDocument doc(32768);
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return false;
  }

  if (doc.containsKey("errors")) {
    Serial.printf("GraphQL error: %s\n",
      doc["errors"][0]["message"].as<const char*>());
    return false;
  }

  JsonObject root = doc["data"];
  JsonArray teams = root["teams"]["nodes"];
  if (teams.size() == 0) {
    Serial.println("No teams found");
    return false;
  }

  // Use the first team
  JsonObject team = teams[0];
  data.teamId   = team["id"].as<String>();
  data.teamName = team["name"].as<String>();

  Serial.printf("Using team: %s\n", data.teamName.c_str());

  // Workflow states — add "(Default)" as first option
  data.states.clear();
  data.states.push_back({"", "(Default)"});
  for (JsonObject s : team["states"]["nodes"].as<JsonArray>()) {
    data.states.push_back({s["id"].as<String>(), s["name"].as<String>()});
  }

  // Members — add "(None)" as first option
  data.members.clear();
  data.members.push_back({"", "(None)"});
  for (JsonObject m : team["members"]["nodes"].as<JsonArray>()) {
    data.members.push_back({m["id"].as<String>(), m["name"].as<String>()});
  }

  // Projects — add "(None)" as first option
  data.projects.clear();
  data.projects.push_back({"", "(None)"});
  for (JsonObject p : root["projects"]["nodes"].as<JsonArray>()) {
    data.projects.push_back({p["id"].as<String>(), p["name"].as<String>()});
  }

  // Labels — add "(None)" as first option
  data.labels.clear();
  data.labels.push_back({"", "(None)"});
  for (JsonObject l : team["labels"]["nodes"].as<JsonArray>()) {
    data.labels.push_back({l["id"].as<String>(), l["name"].as<String>()});
  }

  return true;
}

// ---------------------------------------------------------------------------
// Create issue
// ---------------------------------------------------------------------------

IssueResult linearCreateIssue(const IssueParams& params) {
  IssueResult result = {false, "", ""};

  DynamicJsonDocument req(2048);
  req["query"] = CREATE_MUTATION;

  JsonObject vars  = req.createNestedObject("variables");
  JsonObject input = vars.createNestedObject("input");
  input["teamId"] = params.teamId;
  input["title"]  = params.title;

  if (params.stateId.length() > 0)    input["stateId"]    = params.stateId;
  if (params.priority > 0)            input["priority"]   = params.priority;
  if (params.assigneeId.length() > 0) input["assigneeId"] = params.assigneeId;
  if (params.projectId.length() > 0)  input["projectId"]  = params.projectId;
  if (params.labelId.length() > 0) {
    JsonArray lids = input.createNestedArray("labelIds");
    lids.add(params.labelId);
  }

  String body;
  serializeJson(req, body);

  Serial.println("Creating issue...");
  String resp = post(body);
  if (resp.isEmpty()) {
    result.error = "No response";
    return result;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, resp);
  if (err) {
    result.error = "Parse error";
    return result;
  }

  if (doc.containsKey("errors")) {
    result.error = doc["errors"][0]["message"].as<String>();
    return result;
  }

  JsonObject ic = doc["data"]["issueCreate"];
  result.success = ic["success"].as<bool>();
  if (result.success) {
    result.identifier = ic["issue"]["identifier"].as<String>();
  } else {
    result.error = "issueCreate returned false";
  }

  return result;
}
