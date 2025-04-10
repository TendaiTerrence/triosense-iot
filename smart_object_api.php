<?php 
include "connection.php"; 

// Set the content type to JSON and handle CORS
header('Content-Type: application/json');
header("Access-Control-Allow-Origin: *"); 
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type, Authorization");

// Determine the HTTP method
$method = $_SERVER['REQUEST_METHOD'];

switch ($method) {
    case 'GET':
        if (isset($_GET['action']) && $_GET['action'] === 'fetchNodeList') {
            fetchNodeList();
        } else {
            fetchSensorData();
        }
        break;

    case 'POST':
        addOrUpdateSensorData();
        break;

    default:
        // Invalid method
        http_response_code(405);
        echo json_encode(['error' => 'Method not allowed']);
        break;
}

/**
 * Function to fetch the list of nodes
 */
function fetchNodeList() {
    global $con;

    $sql = "SELECT DISTINCT DeviceName, DeviceID FROM smartdevices ORDER BY DeviceName ASC";
    $result = mysqli_query($con, $sql);

    if ($result) {
        $nodes = [];
        while ($row = mysqli_fetch_assoc($result)) {
            $nodes[] = [
                'DeviceID' => $row['DeviceID'],
                'DeviceName' => $row['DeviceName']
            ];
        }
        echo json_encode($nodes);
    } else {
        echo json_encode(['error' => 'Failed to fetch node list']);
    }

    $con->close();
}

/**
 * Function to fetch sensor data from the database
 */
function fetchSensorData() {
    global $con;

    // Check for a valid database connection
    if (!$con) {
        echo json_encode(["error" => "Database connection failed"]);
        return;
    }

    $limit = isset($_GET['limit']) ? intval($_GET['limit']) : 50;
    $nodeID = isset($_GET['DeviceID']) ? intval($_GET['DeviceID']) : null;

    $sql = "SELECT DISTINCT sr.Timestamp, sr.Temperature, sr.Humidity, sr.LightIntensity, sn.DeviceName 
            FROM sensorreadings sr 
            JOIN smartdevices sn ON sr.DeviceID = sn.DeviceID ";

    if ($nodeID) {
        $sql .= "WHERE sr.DeviceID = ? ";
    }

    $sql .= "ORDER BY sr.ReadingID DESC LIMIT ?";

    $stmt = mysqli_prepare($con, $sql);

    if ($stmt) {
        if ($nodeID) {
            mysqli_stmt_bind_param($stmt, "ii", $nodeID, $limit);
        } else {
            mysqli_stmt_bind_param($stmt, "i", $limit);
        }
        mysqli_stmt_execute($stmt);
        $result = mysqli_stmt_get_result($stmt);

        $records = [];
        while ($row = $result->fetch_assoc()) {
            $records[] = [
                "Timestamp" => $row["Timestamp"],
                "Temperature" => $row["Temperature"],
                "Humidity" => $row["Humidity"],
                "LightIntensity" => $row["LightIntensity"],
                "DeviceName" => $row["DeviceName"]
            ];
        }

        // Return the results as JSON
        echo json_encode(array_values($records)); 
        mysqli_stmt_close($stmt);
    } else {
        // Return error if the statement preparation fails
        echo json_encode(["error" => "Failed to prepare SQL statement"]);
        http_response_code(500); // Internal Server Error
    }

    // Close the database connection
    mysqli_close($con);
}


/**
 * Function to add or update sensor data in the database
 */
function addOrUpdateSensorData() {
    global $con;

    $data = json_decode(file_get_contents('php://input'), true);

    if (!empty($data)) {
        $DeviceID = $data['deviceID'];
        $DeviceName = $data['deviceName'];
        $DLocation = $data['dLocation'];
        $Temperature = $data['Temperature'] ?? null;
        $Humidity = $data['Humidity'] ?? null;
        $LightIntensity = $data['LightIntensity'] ?? null;

        // Check if DeviceID exists in smartdevices table
        $check_sql = "SELECT DeviceName, DLocation FROM smartdevices WHERE DeviceID = ?";
        $check_stmt = mysqli_prepare($con, $check_sql);
        mysqli_stmt_bind_param($check_stmt, "i", $DeviceID);
        mysqli_stmt_execute($check_stmt);
        mysqli_stmt_store_result($check_stmt);

        if (mysqli_stmt_num_rows($check_stmt) > 0) {
            // Device exists, fetch current values
            mysqli_stmt_bind_result($check_stmt, $currentDeviceName, $currentDLocation);
            mysqli_stmt_fetch($check_stmt);
            mysqli_stmt_close($check_stmt);

            // Update device details if they have changed
            if ($DeviceName !== $currentDeviceName || $DLocation !== $currentDLocation) {
                $update_sql = "UPDATE smartdevices SET DeviceName = ?, DLocation = ? WHERE DeviceID = ?";
                $update_stmt = mysqli_prepare($con, $update_sql);
                if ($update_stmt) {
                    mysqli_stmt_bind_param($update_stmt, "ssi", $DeviceName, $DLocation, $DeviceID);
                    mysqli_stmt_execute($update_stmt);
                    mysqli_stmt_close($update_stmt);
                }
            }
        } else {
            // Add new device
            $register_sql = "INSERT INTO smartdevices (DeviceID, DeviceName, DLocation) VALUES (?, ?, ?)";
            $register_stmt = mysqli_prepare($con, $register_sql);

            if ($register_stmt) {
                mysqli_stmt_bind_param($register_stmt, "iss", $DeviceID, $DeviceName, $DLocation);
                mysqli_stmt_execute($register_stmt);
                mysqli_stmt_close($register_stmt);
            } else {
                echo json_encode(["error" => "Failed to prepare device registration query"]);
                return;
            }
        }

        // Add sensor data to the sensorreadings table if provided
        if ($Temperature !== null && $Humidity !== null && $LightIntensity !== null) {
            $sensor_sql = "INSERT INTO sensorreadings (DeviceID, Temperature, Humidity, LightIntensity) 
                           VALUES (?, ?, ?, ?)";
            $sensor_stmt = mysqli_prepare($con, $sensor_sql);

            if ($sensor_stmt) {
                mysqli_stmt_bind_param($sensor_stmt, "iddd", $DeviceID, $Temperature, $Humidity, $LightIntensity);
                mysqli_stmt_execute($sensor_stmt);

                if (mysqli_stmt_affected_rows($sensor_stmt) > 0) {
                    echo json_encode(["message" => "Data updated successfully"]);
                } else {
                    echo json_encode(["error" => "Failed to add sensor data"]);
                }

                mysqli_stmt_close($sensor_stmt);
            } else {
                echo json_encode(["error" => "Failed to prepare sensor data query"]);
            }
        } else {
            echo json_encode(["message" => "Device information updated without sensor data"]);
        }
    } else {
        echo json_encode(["error" => "Invalid data"]);
    }
}
