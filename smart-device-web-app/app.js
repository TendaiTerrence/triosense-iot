document.addEventListener('DOMContentLoaded', () => {
    const API_URL = 'http://localhost/final/smart_object_api.php';

    // Display error messages in the UI
    function displayError(message) {
        const errorContainer = document.getElementById('error-container');
        if (!errorContainer) {
            console.error(message);
            return;
        }
        errorContainer.textContent = message;
        errorContainer.style.display = 'block';
        setTimeout(() => (errorContainer.style.display = 'none'), 5000);
    }

    // Display loading spinner
    function toggleLoadingSpinner(show) {
        const spinner = document.getElementById('loading-spinner');
        if (spinner) {
            spinner.style.display = show ? 'block' : 'none';
        }
    }

    // Retry logic for API requests
    async function fetchWithRetry(url, options = {}, retries = 3, delay = 1000) {
        try {
            const response = await fetch(url, options);
            if (!response.ok) throw new Error(`Failed with status: ${response.status}`);
            return await response.json();
        } catch (error) {
            if (retries > 0) {
                console.warn(`Retrying... Attempts left: ${retries}`);
                await new Promise(res => setTimeout(res, delay));
                return fetchWithRetry(url, options, retries - 1, delay);
            }
            throw new Error('Max retries reached. ' + error.message);
        }
    }

    // Fetch and display the latest readings
    async function fetchLatestReadings() {
        try {
            toggleLoadingSpinner(true);
            const data = await fetchWithRetry(`${API_URL}?action=fetchSensorData&limit=5`);
            const tableBody = document.getElementById('latest-readings');
            tableBody.innerHTML = ''; // Clear existing rows

            data.forEach(reading => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${reading.DeviceName}</td>
                    <td>${reading.Temperature}</td>
                    <td>${reading.Humidity}</td>
                    <td>${reading.LightIntensity}</td>
                    <td>${reading.Timestamp}</td>
                `;
                tableBody.appendChild(row);
            });
        } catch (error) {
            displayError('Error fetching latest readings: ' + error.message);
        } finally {
            toggleLoadingSpinner(false);
        }
    }

    // Initialize Chart.js for sensor trends
    async function initSensorChart() {
        try {
            toggleLoadingSpinner(true);
            const data = await fetchWithRetry(`${API_URL}?action=fetchSensorData&limit=25`);
            const labels = data.map(reading => reading.Timestamp);
            const temperatures = data.map(reading => reading.Temperature);
            const humidities = data.map(reading => reading.Humidity);
            const lightIntensities = data.map(reading => reading.LightIntensity);

            new Chart(document.getElementById('sensor-chart'), {
                type: 'line',
                data: {
                    labels,
                    datasets: [
                        {
                            label: 'Temperature (°C)',
                            data: temperatures,
                            borderColor: 'orange',
                            fill: false
                        },
                        {
                            label: 'Humidity (%)',
                            data: humidities,
                            borderColor: 'blue',
                            fill: false
                        },
                        {
                            label: 'Light Intensity',
                            data: lightIntensities,
                            borderColor: 'green',
                            fill: false
                        }
                    ]
                },
                options: {
                    responsive: true,
                    scales: {
                        x: { title: { display: true, text: 'Timestamp' } },
                        y: { title: { display: true, text: 'Value' } }
                    }
                }
            });
        } catch (error) {
            displayError('Error initializing sensor chart: ' + error.message);
        } finally {
            toggleLoadingSpinner(false);
        }
    }

    // Add a new IoT node
    document.getElementById('add-node-form').addEventListener('submit', async event => {
        event.preventDefault();

        const deviceID = document.getElementById('device-id').value.trim();
        const deviceName = document.getElementById('device-name').value.trim();
        const deviceLocation = document.getElementById('device-location').value.trim();

        if (!deviceID || !deviceName || !deviceLocation) {
            displayError('All fields are required.');
            return;
        }

        try {
            toggleLoadingSpinner(true);
            const response = await fetch(`${API_URL}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: 'addOrUpdateSensorData', deviceID, deviceName, dLocation: deviceLocation })
            });

            const result = await response.json();
            if (response.ok) {
                alert('Node added successfully!');
                document.getElementById('add-node-form').reset();
                populateNodeDropdown(); // Refresh the dropdown
            } else {
                displayError(`Error: ${result.error || 'Unknown error occurred'}`);
            }
        } catch (error) {
            displayError('Error adding node: ' + error.message);
        } finally {
            toggleLoadingSpinner(false);
        }
    });

    // Fetch and display data for a specific node
    async function fetchNodeData(nodeID) {
        try {
            toggleLoadingSpinner(true);
            const data = await fetchWithRetry(`${API_URL}?action=fetchNodeData&DeviceID=${nodeID}`);
            const tableBody = document.getElementById('node-data-table');
            tableBody.innerHTML = '';
            const timestamps = [];
            const temperatures = [];
            const humidities = [];
            const lightIntensities = [];

            data.forEach((reading) => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${reading.Timestamp}</td>
                    <td>${reading.Temperature}</td>
                    <td>${reading.Humidity}</td>
                    <td>${reading.LightIntensity}</td>
                `;
                tableBody.appendChild(row);

                timestamps.push(reading.Timestamp);
                temperatures.push(reading.Temperature);
                humidities.push(reading.Humidity);
                lightIntensities.push(reading.LightIntensity);
            });

            // Get the canvas element for the chart
            const ctx = document.getElementById('node-data-chart').getContext('2d');

            // If there's an existing chart, destroy it
            if (window.nodeDataChart) {
                window.nodeDataChart.destroy();
            }

            // Create a new chart
            window.nodeDataChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: timestamps,
                    datasets: [
                        {
                            label: 'Temperature (°C)',
                            data: temperatures,
                            borderColor: 'orange',
                            fill: false,
                        },
                        {
                            label: 'Humidity (%)',
                            data: humidities,
                            borderColor: 'blue',
                            fill: false,
                        },
                        {
                            label: 'Light Intensity',
                            data: lightIntensities,
                            borderColor: 'green',
                            fill: false,
                        },
                    ],
                },
                options: {
                    responsive: true,
                    scales: {
                        x: { title: { display: true, text: 'Timestamp' } },
                        y: { title: { display: true, text: 'Value' } },
                    },
                },
            });
        } catch (error) {
            displayError('Error fetching node data: ' + error.message);
        } finally {
            toggleLoadingSpinner(false);
        }
    }


    // Populate node dropdown with unique devices
    async function populateNodeDropdown() {
        try {
            const nodes = await fetchWithRetry(`${API_URL}?action=fetchNodeList`);
            const dropdown = document.getElementById('node-id');
            dropdown.innerHTML = ''; // Clear existing options

            nodes.forEach(node => {
                const option = document.createElement('option');
                option.value = node.DeviceID;
                option.textContent = node.DeviceName;
                dropdown.appendChild(option);
            });
        } catch (error) {
            displayError('Error populating node dropdown: ' + error.message);
        }
    }

    // Event listener for "View Data" button
    document.getElementById('view-data-form').addEventListener('submit', event => {
        event.preventDefault();
        const selectedNodeID = document.getElementById('node-id').value;
        if (selectedNodeID) {
            fetchNodeData(selectedNodeID);
        } else {
            displayError('Please select a node to view data.');
        }
    });

    // Load initial data and set up event listeners
    fetchLatestReadings();
    initSensorChart();
    populateNodeDropdown();

    // Refresh the latest readings every 6 seconds
    setInterval(fetchLatestReadings, 6000);
});
