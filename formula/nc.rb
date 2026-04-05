class Nc < Formula
  desc "NC â€” The AI Language. Write AI APIs in plain English."
  homepage "https://nc.devheallabs.in"
  url "https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v1.0.0.tar.gz"
  # SHA256 must be filled AFTER creating the GitHub release tag.
  # Run this to compute:
  #   curl -sL https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v1.0.0.tar.gz | shasum -a 256
  # Then paste the hash below. Homebrew install will fail without this.
  sha256 ""
  license "Apache-2.0"
  head "https://github.com/devheallabs-ai/nc-lang.git", branch: "main"

  depends_on "curl"

  # Pre-built bottles for faster installation (populated by CI)
  # bottle do
  #   root_url "https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0"
  #   sha256 cellar: :any_skip_relocation, arm64_sonoma: ""
  #   sha256 cellar: :any_skip_relocation, sonoma:       ""
  #   sha256 cellar: :any_skip_relocation, x86_64_linux: ""
  # end

  def install
    cd "engine" do
      system "make", "clean"
      system "make"
      bin.install "build/nc"
    end

    pkgshare.install "nc_ai_providers.json" if File.exist?("nc_ai_providers.json")
    pkgshare.install "lib" if File.directory?("lib")
    pkgshare.install "examples" if File.directory?("examples")

  end

  def caveats
    <<~EOS
      NC has been installed successfully!

      To use AI features, set your API key:
        export NC_AI_KEY="your-api-key"

      Quick start:
        nc version
        nc "show 42 + 8"
        nc serve service.nc

      Documentation: https://nc.devheallabs.in/docs
      License: Apache 2.0 â€” https://nc.devheallabs.in/license
    EOS
  end

  test do
    assert_match "NC", shell_output("#{bin}/nc version")

    (testpath/"hello.nc").write('show "hello from NC"')
    assert_match "hello from NC", shell_output("#{bin}/nc run #{testpath}/hello.nc")

    (testpath/"math.nc").write("show 2 + 3")
    assert_match "5", shell_output("#{bin}/nc run #{testpath}/math.nc")
  end
end

