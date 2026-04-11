"""
Step 1: Train a model in Python (you do this once).

This creates a simple classifier that predicts if someone
will buy a product based on age and salary.

Run: python3 train_model.py
Output: model.pkl
"""

from sklearn.tree import DecisionTreeClassifier
import pickle, json

# Training data: [age, salary] -> will_buy (0 or 1)
X = [
    [22, 30000], [25, 35000], [30, 50000], [35, 60000],
    [40, 80000], [45, 90000], [50, 70000], [28, 40000],
    [33, 55000], [55, 100000], [20, 20000], [60, 120000],
]
y = [0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1]

model = DecisionTreeClassifier()
model.fit(X, y)

# Save the trained model
with open("model.pkl", "wb") as f:
    pickle.dump(model, f)

print("Model trained and saved to model.pkl")
print(f"Test prediction for [30, 50000]: {model.predict([[30, 50000]])[0]}")
