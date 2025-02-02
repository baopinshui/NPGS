import os
import shutil

def copy_files_with_structure(src_dir, dst_dir, extensions):
    for root, _, files in os.walk(src_dir):
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                src_file_path = os.path.join(root, file)
                relative_path = os.path.relpath(root, src_dir)
                dst_file_dir = os.path.join(dst_dir, relative_path)
                os.makedirs(dst_file_dir, exist_ok=True)
                shutil.copy2(src_file_path, dst_file_dir)
                print(f"Copied {src_file_path} to {dst_file_dir}")

if __name__ == "__main__":
    source_directory = os.path.abspath(os.path.join(os.path.dirname(__file__), '../Sources'))
    destination_directory = os.path.abspath(os.path.join(os.path.dirname(__file__), '../Includes'))
    file_extensions = ['.h', '.inl', '.hpp']

    copy_files_with_structure(source_directory, destination_directory, file_extensions)