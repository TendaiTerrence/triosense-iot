<?php
$SERVER = 'localhost';
$USERNAME = 'root';
$PASSWORD = '';
$DB_NAME = 'iot_project';

$con = new mysqli($SERVER, $USERNAME, $PASSWORD, $DB_NAME) or die("Could not establish connection to database");

if ($con->connect_error) {
    die("Connection failed: " . $con->connect_error);
}