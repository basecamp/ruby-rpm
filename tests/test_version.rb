require 'test/unit'
require 'rpm'

class RPM_Version_Tests < Test::Unit::TestCase
  def setup
    @a = RPM::Version.new( '1.0.0-0.1m' )
    @b = RPM::Version.new( '0.9.0-1m' )
    @c = RPM::Version.new( '1.0.0-0.11m' )
    @d = RPM::Version.new( '0.9.0-1m', 1)
  end

  def test_version_compare
    assert( @a > @b )
    assert( @a < @c )
    assert( @a < @d )
  end

  def test_version_newer?
    assert( @a.newer?(@b) )
    assert( @c.newer?(@a) )
    assert( @d.newer?(@a) )
  end

  def test_version_older?
    assert( @b.older?(@a) )
    assert( @a.older?(@c) )
    assert( @a.older?(@d) )
  end

  def test_vre
    assert_equal( '0.9.0', @d.v )
    assert_equal( '1m', @d.r )
    assert_equal( 1, @d.e )
  end

  def test_to_s
    assert_equal( '0.9.0-1m', @b.to_s )
    assert_equal( '0.9.0-1m', @d.to_s )
  end

  def test_to_vre
    assert_equal( '0.9.0-1m', @b.to_vre )
    assert_equal( '1:0.9.0-1m', @d.to_vre )
  end

  def test_inspect
      v = RPM::Version.new("1", "2", 3)
      assert_equal(3, v.e)
      assert_equal('#<RPM::Version v="1", r="2", e=3>', v.inspect)
  end
end
