import os
import argparse

def instantiate_template(template_path, output_path, parameters):
    """
    Reads a template file, replaces placeholders with provided parameters,
    and writes the result to an output file.

    Args:
        template_path: Path to the template file
        output_path: Path to write the output file
        parameters: Dictionary of parameters to replace in the template
    """
    # Read the template file
    with open(template_path, 'r') as f:
        template_content = f.read()

    # Replace all placeholders with provided parameters
    for key, value in parameters.items():
        placeholder = f"${key}$"
        template_content = template_content.replace(placeholder, str(value))

    # Write the resulting content to the output file
    with open(output_path, 'w') as f:
        f.write(template_content)

    print(f"Template instantiated successfully: {output_path}")

